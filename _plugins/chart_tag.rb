# Liquid tag for placing ECharts charts in posts.
# Usage: {% chart %} (first chart) or {% chart myid %} (chart with id "myid").
# Chart options live in the post's `chart:` / `charts:` front matter;
# see _includes/metadata-hook.html and assets/js/echarts-renderer.js.
module Jekyll
  class ChartTag < Liquid::Tag
    def initialize(tag_name, markup, tokens)
      super
      @chart_id = markup.strip
    end

    def render(_context)
      if @chart_id.empty?
        '<div data-chart></div>'
      else
        %(<div data-chart="#{@chart_id}"></div>)
      end
    end
  end
end

Liquid::Template.register_tag('chart', Jekyll::ChartTag)
