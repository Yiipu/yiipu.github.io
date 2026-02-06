---
title: "在AWS ECS部署Clawdbot并配置Qwen+Telegram+Brave搜索"
date: 2025-02-05 15:00:00 +0800
last_modified_at: 2025-02-05 15:00:00 +0800
categories:
  - 环境配置
  - AWS
  - Chatbot
tags:
  - clawdbot
  - aws ecs
  - qwen
  - telegram
  - brave search
  - docker
description: "详细教程介绍如何在AWS ECS上部署Clawdbot机器人，并配置Qwen模型、Telegram频道和Brave搜索功能"
---

# 在AWS ECS部署Clawdbot并配置Qwen+Telegram+Brave搜索

## 概述

Clawdbot是一个强大的多渠道AI助手框架，支持多种消息平台和AI模型。本文将详细介绍如何在AWS ECS（Elastic Container Service）上部署Clawdbot，并配置Qwen模型、Telegram集成和Brave搜索功能。

## 准备工作

在开始之前，您需要准备以下资源：

1. AWS账户及访问权限
2. AWS CLI已安装并配置
3. Docker已安装
4. AWS ECS集群
5. Qwen Portal账户（用于免费的Qwen Coder模型）
6. Telegram Bot Token
7. Brave Search API密钥

## 第一步：创建ECS任务定义

首先，我们需要创建一个ECS任务定义，用于运行Clawdbot容器。创建`clawdbot-task-definition.json`文件：

```json
{
  "family": "clawdbot-service",
  "executionRoleArn": "arn:aws:iam::YOUR_ACCOUNT_ID:role/ecsTaskExecutionRole",
  "taskRoleArn": "arn:aws:iam::YOUR_ACCOUNT_ID:role/ecsTaskRole",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "1024",
  "memory": "2048",
  "runtimePlatform": {
    "cpuArchitecture": "X86_64",
    "operatingSystemFamily": "LINUX"
  },
  "containerDefinitions": [
    {
      "name": "clawdbot-container",
      "image": "node:22-bookworm",
      "cpu": 1024,
      "memory": 1536,
      "essential": true,
      "portMappings": [
        {
          "containerPort": 18789,
          "protocol": "tcp"
        }
      ],
      "environment": [
        {
          "name": "NODE_ENV",
          "value": "production"
        },
        {
          "name": "CLAWDBOT_GATEWAY_BIND",
          "value": "0.0.0.0"
        },
        {
          "name": "CLAWDBOT_GATEWAY_PORT",
          "value": "18789"
        },
        {
          "name": "CLAWDBOT_GATEWAY_TOKEN",
          "value": "your-gateway-token-here"
        },
        {
          "name": "TELEGRAM_BOT_TOKEN",
          "value": "your-telegram-bot-token"
        },
        {
          "name": "BRAVE_API_KEY",
          "value": "your-brave-api-key"
        },
        {
          "name": "HOME",
          "value": "/home/node"
        }
      ],
      "mountPoints": [
        {
          "sourceVolume": "clawdbot-config",
          "containerPath": "/home/node/.clawdbot"
        },
        {
          "sourceVolume": "clawdbot-workspace",
          "containerPath": "/home/node/clawd"
        }
      ],
      "logConfiguration": {
        "logDriver": "awslogs",
        "options": {
          "awslogs-group": "/ecs/clawdbot-service",
          "awslogs-region": "us-east-1",
          "awslogs-stream-prefix": "ecs"
        }
      }
    }
  ],
  "volumes": [
    {
      "name": "clawdbot-config",
      "efsVolumeConfiguration": {
        "fileSystemId": "fs-your-efs-id",
        "rootDirectory": "/clawdbot/config"
      }
    },
    {
      "name": "clawdbot-workspace",
      "efsVolumeConfiguration": {
        "fileSystemId": "fs-your-efs-id",
        "rootDirectory": "/clawdbot/workspace"
      }
    }
  ]
}
```

## 第二步：构建自定义Docker镜像

为了确保Clawdbot能正常运行，我们需要创建一个包含所有必要依赖的自定义Docker镜像。创建`Dockerfile`：

```dockerfile
FROM node:22-bookworm

# 安装必要的系统工具
RUN apt-get update && apt-get install -y \
    socat \
    curl \
    ca-certificates \
    git \
    && rm -rf /var/lib/apt/lists/*

# 安装Clawdbot
RUN npm install -g @clawbot/clawdbot

# 安装额外的二进制工具（如果需要）
# 示例：Gmail CLI
RUN curl -L https://github.com/steipete/gog/releases/latest/download/gog_Linux_x86_64.tar.gz \
  | tar -xz -C /usr/local/bin && chmod +x /usr/local/bin/gog

# 创建非root用户
RUN groupadd -r nodejs && useradd -r -g nodejs node

# 设置工作目录
WORKDIR /app

# 切换到非root用户
USER node

# 创建必要的目录
RUN mkdir -p /home/node/.clawdbot
RUN mkdir -p /home/node/clawd

EXPOSE 18789

CMD ["clawdbot", "gateway", "--bind", "0.0.0.0", "--port", "18789"]
```

然后构建并推送镜像到ECS：

```bash
# 构建镜像
docker build -t your-registry-url/clawdbot:latest .

# 登录到ECR
aws ecr get-login-password --region us-east-1 | docker login --username AWS --password-stdin your-registry-url

# 推送镜像
docker push your-registry-url/clawdbot:latest
```

更新任务定义中的镜像地址：

```json
{
  "image": "your-registry-url/clawdbot:latest"
}
```

## 第三步：配置Qwen模型

Clawdbot支持通过Qwen Portal的OAuth流程使用Qwen模型。以下是配置步骤：

### 启用Qwen插件

```bash
# 在容器启动后执行此命令
clawdbot plugins enable qwen-portal-auth
```

### 认证Qwen

```bash
# 执行认证流程
clawdbot models auth login --provider qwen-portal --set-default
```

这将运行Qwen设备码OAuth流程，并在`models.json`中写入提供程序条目（以及`qwen`别名用于快速切换）。

### Qwen模型ID

- `qwen-portal/coder-model` - Qwen Coder模型
- `qwen-portal/vision-model` - Qwen Vision模型

切换模型：
```bash
clawdbot models set qwen-portal/coder-model
```

## 第四步：配置Telegram集成

### 创建Telegram Bot

1. 在Telegram中与**@BotFather**对话
2. 运行`/newbot`，按照提示创建新机器人
3. 复制生成的令牌并安全保存

### 配置Clawdbot

在Clawdbot配置中添加以下内容：

```json
{
  "channels": {
    "telegram": {
      "enabled": true,
      "botToken": "YOUR_TELEGRAM_BOT_TOKEN",
      "dmPolicy": "pairing",
      "groups": {
        "*": {
          "requireMention": true
        }
      }
    }
  }
}
```

或者通过环境变量设置：
```bash
export TELEGRAM_BOT_TOKEN=your_bot_token_here
```

### Telegram配置选项

- `dmPolicy`: 控制DM访问策略（pairing, allowlist, open, disabled）
- `groupPolicy`: 控制群组访问策略（open, allowlist, disabled）
- `groups.*.requireMention`: 控制是否需要提及机器人才能响应

## 第五步：配置Brave搜索

### 获取Brave Search API密钥

1. 访问 https://brave.com/search/api/ 创建Brave Search API账户
2. 在仪表板中选择**Data for Search**计划并生成API密钥
3. 将密钥存储在配置中或设置`BRAVE_API_KEY`环境变量

### 配置示例

```json
{
  "tools": {
    "web": {
      "search": {
        "provider": "brave",
        "apiKey": "YOUR_BRAVE_API_KEY",
        "maxResults": 5,
        "timeoutSeconds": 30
      }
    }
  }
}
```

### 重要注意事项

- 数据AI计划**不**兼容`web_search`
- Brave提供免费套餐加付费计划，检查Brave API门户了解当前限制

## 第六步：部署到ECS

### 注册任务定义

```bash
aws ecs register-task-definition --cli-input-json file://clawdbot-task-definition.json
```

### 创建服务

```bash
aws ecs create-service \
  --cluster your-cluster-name \
  --service-name clawdbot-service \
  --task-definition clawdbot-service:1 \
  --desired-count 1 \
  --launch-type FARGATE \
  --network-configuration "awsvpcConfiguration={subnets=[subnet-xxxxx],securityGroups=[sg-xxxxx],assignPublicIp=ENABLED}"
```

### 创建负载均衡器（可选）

如果您希望从外部访问Clawdbot控制界面，可以创建Application Load Balancer：

```bash
# 创建目标组
aws elbv2 create-target-group \
  --name clawdbot-target-group \
  --protocol HTTP \
  --port 18789 \
  --vpc-id your-vpc-id \
  --health-check-path "/" \
  --health-check-protocol HTTP

# 创建负载均衡器
aws elbv2 create-load-balancer \
  --name clawdbot-load-balancer \
  --subnets subnet-xxxxx subnet-yyyyy \
  --security-groups sg-xxxxx
```

## 第七步：持久化配置

由于容器是临时的，我们需要确保Clawdbot的配置和状态能够持久保存：

1. 使用Amazon EFS存储配置文件
2. 配置卷挂载到`/home/node/.clawdbot`和`/home/node/clawd`
3. 确保适当的权限设置

### 持久化组件

| 组件 | 位置 | 持久化机制 | 说明 |
|------|------|------------|------|
| 网关配置 | `/home/node/.clawdbot/` | EFS卷挂载 | 包括`clawdbot.json`、令牌 |
| 模型认证 | `/home/node/.clawdbot/` | EFS卷挂载 | OAuth令牌、API密钥 |
| 技能配置 | `/home/node/.clawdbot/skills/` | EFS卷挂载 | 技能级状态 |
| 代理工作区 | `/home/node/clawd/` | EFS卷挂载 | 代码和代理工件 |
| WhatsApp会话 | `/home/node/.clawdbot/` | EFS卷挂载 | 保留QR登录 |
| Gmail密钥环 | `/home/node/.clawdbot/` | EFS卷挂载 | 需要`GOG_KEYRING_PASSWORD` |

## 第八步：监控和维护

### 日志管理

配置CloudWatch日志组来收集容器日志：

```json
{
  "logConfiguration": {
    "logDriver": "awslogs",
    "options": {
      "awslogs-group": "/ecs/clawdbot-service",
      "awslogs-region": "us-east-1",
      "awslogs-stream-prefix": "ecs"
    }
  }
}
```

### 健康检查

定期检查服务状态：

```bash
aws ecs describe-services --cluster your-cluster-name --services clawdbot-service
```

### 自动扩展

根据需要配置自动扩展策略：

```bash
aws application-autoscaling register-scalable-target \
  --service-namespace ecs \
  --scalable-dimension ecs:service:DesiredCount \
  --resource-id service/your-cluster-name/clawdbot-service \
  --min-capacity 1 \
  --max-capacity 3
```

## 故障排除

### 常见问题

1. **Telegram机器人不响应**：
   - 检查Bot Token是否正确
   - 确认Telegram隐私设置是否关闭
   - 检查防火墙和网络连接

2. **Qwen模型无法使用**：
   - 确认插件已启用：`clawdbot plugins enable qwen-portal-auth`
   - 检查认证状态：`clawdbot models auth login --provider qwen-portal --set-default`

3. **Brave搜索API错误**：
   - 验证API密钥是否有效
   - 检查配额是否已满

4. **容器启动失败**：
   - 检查IAM角色权限
   - 验证环境变量设置
   - 查看CloudWatch日志获取详细错误信息

### 性能优化

1. 根据实际使用情况调整CPU和内存分配
2. 配置适当的健康检查参数
3. 使用缓存机制减少重复请求
4. 定期清理日志和临时文件

## 总结

通过以上步骤，您已经成功在AWS ECS上部署了Clawdbot，并配置了Qwen模型、Telegram集成和Brave搜索功能。这种架构提供了高可用性、可扩展性和安全性，适合生产环境使用。

记得定期监控服务状态，备份重要配置，并根据使用需求调整资源配置。Clawdbot的强大功能结合AWS ECS的弹性能力，为您提供了可靠的AI助手服务。

> **注意**: 在生产环境中，请确保所有敏感信息（如API密钥、令牌等）都通过安全的方式进行管理和传输。