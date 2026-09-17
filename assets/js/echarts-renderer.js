/**
 * ECharts renderer for Jekyll Chirpy.
 * Pattern from linsnotes' Chirpy Chart.js integration (community, 2026-03),
 * with the echarts-setup logic from al-folio (al_charts).
 * Reads window.chartConfig (injected by _includes/metadata-hook.html when the
 * page declares `chart:` / `charts:` front matter), finds placement divs
 * ({% chart %} -> <div data-chart>), and renders one ECharts instance each.
 */
(function () {
  var configs = window.chartConfig;
  if (!configs || !configs.length) return;

  var chartDivs = document.querySelectorAll("[data-chart]");
  if (!chartDivs.length) return;

  function resolveConfig(div) {
    var id = div.getAttribute("data-chart");
    if (!id || id === "") return configs[0];
    for (var j = 0; j < configs.length; j++) {
      if (configs[j].id === id) return configs[j];
    }
    return null;
  }

  function renderChart(div, config) {
    var container = document.createElement("div");
    container.style.width = "100%";
    container.style.height = config.height ? config.height + "px" : "420px";
    div.appendChild(container);

    var isDark =
      document.documentElement.getAttribute("mode") === "dark" ||
      (window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches &&
        document.documentElement.getAttribute("mode") !== "light");

    var chart = echarts.init(container, isDark ? "dark" : null);
    chart.setOption(config.option || config.data || {});

    window.addEventListener("resize", function () {
      chart.resize();
    });
  }

  for (var i = 0; i < chartDivs.length; i++) {
    var div = chartDivs[i];
    var config = resolveConfig(div);
    if (config) renderChart(div, config);
  }
})();
