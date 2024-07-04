---
title: Next.js 全栈开发踩坑记
categories:
  - 开发
  - Web
tags:
  - NodeJS
  - ReactJS
  - NextJS
  - front-end
  - back-end
description: 用 next.js 开发了一个简单的网站, 涉及到 数据库 \ socket \ RESTful API 等等, 分享以下踩过的坑罢.
---

欢迎查看 [源码](https://github.com/Yiipu/Checkdown) 以及 [网站](https://checkdown.azurewebsites.net/).

如果不想注册, 可以使用下面的示例账户.

```
user@example.com
```
```
Abc123456
```

## 数据获取

> Whenever possible, we recommend fetching data on the server with Server Components. 

`Next.js` 如是建议. 那么, 具体怎么实现, 采取什么方式实现, 哪些数据不适合服务端获取呢?

### 服务端

正如 `Next.js` 的建议, 几乎所有的数据获取都应该在服务端进行.

- 支持 `fetch` 的数据源: 在服务端使用 `Next.js` 扩展的 `fetch()`. 这种方式提供了缓存和重新验证.
- 不支持 `fetch` 的数据源: 在服务端使用第三方库. 这种情况下, 虽然数据层面的缓存可能不能保证(取决于第三方库), 但是仍然可以在静态路由段上得到路由层面的缓存的重新验证(对于动态路由段, 参考 [generateStaticParams](https://nextjs.org/docs/app/api-reference/functions/generate-static-params)). 官方文档建议使用 `React` 的 `cache()` 提供更细致的缓存(函数层面的), 不过这个功能在本文编辑时还处于 Canary 阶段.

### 客户端

对于触发获取需要用户交互的数据 (如下拉加载), 只能在客户端获取. 不过, 这并不意味着放弃缓存或是对数据源的控制, 你仍然可以通过 `server action` 或者 `route handler`  来 "代理" 客户端的请求.

- `server action`: 这是 [`Next.js 14`](https://nextjs.org/blog/next-14#server-actions-stable) 的新功能, 我的理解是将增强的 `POST` 包装成可以在客户端 (和服务端) 调用的函数. 具体的功能和限制参考 [`Server Actions`](https://nextjs.org/docs/app/building-your-application/data-fetching/server-actions-and-mutations#behavior). 对于不需要暴露 API 的应用, 这种方法不仅方便而且提供了渐进式增强以及类型安全.
- `route handler` : 定义 API 路由. 适用于按照传统的后端开发模式, 开发一整套 API 的情况.

### 一种特殊情形

我的应用是一个 markdown 预览器，因为我希望在服务端将 markdown 转化为 jsx，因此文件的获取必须在服务端且数据首先到达服务端. 

### 数据库

数据库最好用单独的模块来管理, 我的应用中, 数据库封装在文件 `pool.js` 中, 并暴露对象 `pool` 和 方法 `handleTransaction(fn)` .

`pool` 是 `mysql2` 提供的 mySQL 的连接池, 当某个函数只需要进行不超过 2 次数据库查询时, 就使用 `pool` 的 `execute(sql,param)` 方法.

`handleTransaction(fn)` 是事务操作的抽象逻辑, 包含了错误处理和回滚, 其接收一个函数作为参数, 这个函数的参数是一个数据库连接 `connection` .

```js
/**
 * Handles a database transaction.
 * @param {Function} fn - A function that accepts a connection as its only argument
 * @throws {Error} Throws an error if getting a connection fails or if the
 *                 transaction fails (either during the execution of `fn` or
 *                 during commit).
 */
async function handleTransaction(fn) {
  const connection = await pool.getConnection()
    .catch((err) => {
      console.error(err);
      throw new Error("Failed to get connetion");
    });
  try {
    await connection.beginTransaction();
    await fn(connection);
    await connection.commit();
  } catch (error) {
    await connection.rollback();
    console.error(error);
    throw new Error("Transaction failed");
  } finally {
    connection.release();
  }
}
```
## 踩坑

### \<Link\>  preload 导致用户意外登出

[prefetch](https://nextjs.org/docs/app/api-reference/components/link#prefetch) 是 \<Link\> 组件的默认行为. 注意在不想预加载的标签 (如 `/api/auth/logout` 🙂) 手动关闭预加载:
```jsx
<Link href="/dashboard" prefetch={false}> 
    Dashboard 
</Link>
```

### next dev 的 hot reloading 导致连接池累积

使用 mysql2 库提供的连接池, 理论上可以重用连接并在高并发时通过排队避免连接过多.

```js
// lib/pool.js
import mysql from "mysql2/promise";
import dotenv from "dotenv";

dotenv.config({ path: ".env.local" });

const createPool = () => {
  try {
    const pool = mysql.createPool({
      host: process.env.DB_HOST,
      port: parseInt(process.env.DB_PORT, 10),
      user: process.env.DB_USER,
      password: process.env.DB_PASSWORD,
      database: process.env.DB_DATABASE,
      connectionLimit: 10,
      waitForConnections: true,
      queueLimit: 10,
    });
    return pool;
  } catch (error) {
    console.log(`Could not connect - ${error}`);
    return null;
  }
};

const pool = createPool();
export { pool };
```

```js
import { pool } from 'lib/pool'
await pool.execute(query,[...params]);
```

然而在调试时, 数据库因为连接过多报错了. 包作者的 GitHub 仓库也有人提类似的 issue, 有人提到可能是热重载的问题.  
热重载的过程中, 有多个连接池实例被创建, 而旧的连接池没有释放其连接, 新的实例又建立了新的连接, 这就导致了问题发生.