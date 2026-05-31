import { hilog } from '@kit.PerformanceAnalysisKit';
import { fetchReadings } from '../api';
import type { Reading, MetricConfig, TimeWindow } from '../models';
import { ALL_METRICS, TIME_WINDOWS, formatMetricTime, formatShortTime, formatValue } from '../models';

const DOMAIN = 0x0000;
const TAG = 'DeviceTrendPage';

interface ChartPoint {
  time: string;
  timestamp: number;
  value: number;
}

@Component
struct TrendChart {
  @Prop color: string = '#2563eb';
  @Prop label: string = '';
  @Prop unit: string = '';
  @Prop precision: number = 0;
  points: ChartPoint[] = [];
  private context: CanvasRenderingContext2D = new CanvasRenderingContext2D(new RenderingContextSettings(true));

  drawChart(): void {
    const ctx = this.context;
    const width: number = ctx.width;
    const height: number = ctx.height;

    ctx.clearRect(0, 0, width, height);

    if (this.points.length === 0) {
      ctx.font = '12px sans-serif';
      ctx.fillStyle = '#9ca3af';
      ctx.textAlign = 'center';
      ctx.fillText('暂无数据', width / 2, height / 2);
      return;
    }

    const paddingLeft: number = 45;
    const paddingRight: number = 12;
    const paddingTop: number = 12;
    const paddingBottom: number = 28;
    const chartWidth: number = width - paddingLeft - paddingRight;
    const chartHeight: number = height - paddingTop - paddingBottom;

    let minVal: number = this.points[0].value;
    let maxVal: number = this.points[0].value;
    let minTime: number = this.points[0].timestamp;
    let maxTime: number = this.points[0].timestamp;

    for (const p of this.points) {
      if (p.value < minVal) minVal = p.value;
      if (p.value > maxVal) maxVal = p.value;
      if (p.timestamp < minTime) minTime = p.timestamp;
      if (p.timestamp > maxTime) maxTime = p.timestamp;
    }

    if (maxVal === minVal) {
      maxVal = minVal + 1;
    }
    const valRange: number = maxVal - minVal;
    const timeRange: number = maxTime - minTime > 0 ? maxTime - minTime : 1;

    // Grid lines
    ctx.strokeStyle = '#e5e7eb';
    ctx.lineWidth = 0.5;
    for (let i = 0; i <= 4; i++) {
      const y: number = paddingTop + (chartHeight * i) / 4;
      ctx.beginPath();
      ctx.moveTo(paddingLeft, y);
      ctx.lineTo(width - paddingRight, y);
      ctx.stroke();
    }

    // Y axis labels
    ctx.font = '10px sans-serif';
    ctx.fillStyle = '#6b7280';
    ctx.textAlign = 'right';
    for (let i = 0; i <= 4; i++) {
      const y: number = paddingTop + (chartHeight * i) / 4;
      const val: number = maxVal - (valRange * i) / 4;
      ctx.fillText(val.toFixed(this.precision), paddingLeft - 4, y + 4);
    }

    // X axis labels
    ctx.textAlign = 'center';
    const labelCount: number = Math.min(this.points.length, 5);
    const step: number = Math.max(1, Math.floor(this.points.length / labelCount));
    for (let i = 0; i < this.points.length; i += step) {
      const x: number = paddingLeft + ((this.points[i].timestamp - minTime) / timeRange) * chartWidth;
      ctx.fillText(formatShortTime(this.points[i].time), x, height - 6);
    }

    // Line
    ctx.strokeStyle = this.color;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let i = 0; i < this.points.length; i++) {
      const x: number = paddingLeft + ((this.points[i].timestamp - minTime) / timeRange) * chartWidth;
      const y: number = paddingTop + ((maxVal - this.points[i].value) / valRange) * chartHeight;
      if (i === 0) {
        ctx.moveTo(x, y);
      } else {
        ctx.lineTo(x, y);
      }
    }
    ctx.stroke();

    // Dots
    ctx.fillStyle = this.color;
    for (const p of this.points) {
      const x: number = paddingLeft + ((p.timestamp - minTime) / timeRange) * chartWidth;
      const y: number = paddingTop + ((maxVal - p.value) / valRange) * chartHeight;
      ctx.beginPath();
      ctx.arc(x, y, 2.5, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  build() {
    Canvas(this.context)
      .width('100%')
      .height(160)
      .onReady(() => {
        this.drawChart();
      })
  }
}

@Component
export struct DeviceTrendPage {
  @Prop deviceId: string = '';
  @State loading: boolean = true;
  @State error: string = '';
  @State selectedWindow: string = '24h';
  @State readings: Reading[] = [];
  @State loadedAt: string = '';

  aboutToAppear(): void {
    this.loadData();
  }

  private async loadData(): Promise<void> {
    this.loading = true;
    this.error = '';

    const activeWindow: TimeWindow = TIME_WINDOWS.find((tw: TimeWindow) => tw.key === this.selectedWindow) ?? TIME_WINDOWS[2];
    const now: number = Date.now();
    const from: string = new Date(now - activeWindow.hours * 60 * 60 * 1000).toISOString();
    const sensorKeys: string[] = ALL_METRICS.map((m: MetricConfig) => m.key);

    try {
      const response = await fetchReadings(this.deviceId, sensorKeys, from, activeWindow.limit);
      this.readings = response.readings;
      this.loadedAt = new Date().toISOString();
      hilog.info(DOMAIN, TAG, `Loaded ${response.readings.length} readings for ${activeWindow.label}`);
    } catch (err) {
      this.error = err instanceof Error ? err.message : '数据加载失败';
      hilog.error(DOMAIN, TAG, `Load error: ${this.error}`);
    } finally {
      this.loading = false;
    }
  }

  private getLatestValue(metricKey: string): number | null {
    let latest: Reading | null = null;
    for (const r of this.readings) {
      if (r.sensor_name === metricKey) {
        if (!latest || r.time > latest.time) {
          latest = r;
        }
      }
    }
    return latest ? latest.value_number : null;
  }

  private getLatestTime(metricKey: string): string {
    let latest: Reading | null = null;
    for (const r of this.readings) {
      if (r.sensor_name === metricKey) {
        if (!latest || r.time > latest.time) {
          latest = r;
        }
      }
    }
    return latest ? latest.time : '';
  }

  private getMetricSeries(metricKey: string): ChartPoint[] {
    const points: ChartPoint[] = [];
    for (const r of this.readings) {
      if (r.sensor_name === metricKey && r.value_number !== null) {
        points.push({
          time: r.time,
          timestamp: new Date(r.time).getTime(),
          value: r.value_number
        });
      }
    }
    points.sort((a: ChartPoint, b: ChartPoint) => a.timestamp - b.timestamp);
    return points;
  }

  private formatCardTime(isoTime: string): string {
    if (isoTime.length === 0) return '暂无数据';
    return formatMetricTime(isoTime);
  }

  private getActiveWindowLabel(): string {
    const tw: TimeWindow | undefined = TIME_WINDOWS.find((tw: TimeWindow) => tw.key === this.selectedWindow);
    return tw ? tw.label : '24小时';
  }

  @Builder
  metricChartCard(metric: MetricConfig) {
    Column() {
      // Header
      Row() {
        Text(metric.icon)
          .fontSize(16)
          .margin({ right: 6 })
        Text(metric.label)
          .fontSize(14)
          .fontColor('#374151')
        Blank()
        Text(this.formatCardTime(this.getLatestTime(metric.key)))
          .fontSize(11)
          .fontColor('#9ca3af')
      }
      .width('100%')
      .margin({ bottom: 6 })

      // Value
      Text(formatValue(this.getLatestValue(metric.key), metric.precision, metric.unit))
        .fontSize(24)
        .fontWeight(FontWeight.Bold)
        .fontColor(metric.color)
        .width('100%')
        .margin({ bottom: 8 })

      // Chart
      TrendChart({
        color: metric.color,
        label: metric.label,
        unit: metric.unit,
        precision: metric.precision,
        points: this.getMetricSeries(metric.key)
      })
    }
    .padding(16)
    .width('100%')
    .backgroundColor('#ffffff')
    .borderRadius(12)
    .shadow({ radius: 4, color: '#1A000000', offsetX: 0, offsetY: 2 })
  }

  build() {
    NavDestination() {
      Column() {
        if (this.loading && this.readings.length === 0) {
          Column() {
            LoadingProgress()
              .width(48)
              .height(48)
              .color('#2563eb')
            Text('正在加载数据...')
              .fontSize(14)
              .fontColor('#666666')
              .margin({ top: 16 })
          }
          .width('100%')
          .height('100%')
          .justifyContent(FlexAlign.Center)
        } else if (this.error.length > 0 && this.readings.length === 0) {
          Column() {
            Text('⚠')
              .fontSize(48)
              .margin({ bottom: 16 })
            Text(this.error)
              .fontSize(16)
              .fontColor('#666666')
              .textAlign(TextAlign.Center)
              .margin({ bottom: 16 })
            Button('重试')
              .type(ButtonType.Capsule)
              .width(120)
              .height(40)
              .backgroundColor('#2563eb')
              .onClick(() => {
                this.loadData();
              })
          }
          .width('100%')
          .height('100%')
          .justifyContent(FlexAlign.Center)
        } else {
          Column() {
            // Header
            Column() {
              Text('历史趋势')
                .fontSize(12)
                .fontColor('#6b7280')
                .margin({ bottom: 4 })
              Text(this.deviceId)
                .fontSize(20)
                .fontWeight(FontWeight.Bold)
                .fontColor('#111827')
                .margin({ bottom: 4 })
              Text(`最近 ${this.getActiveWindowLabel()} 环境与光照数据`)
                .fontSize(13)
                .fontColor('#6b7280')
            }
            .width('100%')
            .padding({ left: 16, right: 16, top: 16, bottom: 8 })

            // Time window selector + refresh
            Row() {
              Flex({ wrap: FlexWrap.Wrap }) {
                ForEach(TIME_WINDOWS, (tw: TimeWindow) => {
                  Button(tw.label)
                    .type(ButtonType.Capsule)
                    .fontSize(12)
                    .height(32)
                    .padding({ left: 12, right: 12 })
                    .margin({ right: 8, bottom: 4 })
                    .backgroundColor(this.selectedWindow === tw.key ? '#2563eb' : '#e5e7eb')
                    .fontColor(this.selectedWindow === tw.key ? '#ffffff' : '#374151')
                    .onClick(() => {
                      if (this.selectedWindow !== tw.key) {
                        this.selectedWindow = tw.key;
                        this.loadData();
                      }
                    })
                }, (tw: TimeWindow) => tw.key)
              }
              Blank()
              Button(this.loading ? '...' : '刷新')
                .type(ButtonType.Capsule)
                .fontSize(12)
                .height(32)
                .width(60)
                .backgroundColor('#2563eb')
                .enabled(!this.loading)
                .onClick(() => {
                  this.loadData();
                })
            }
            .width('100%')
            .padding({ left: 16, right: 16, bottom: 8 })
            .alignItems(VerticalAlign.Center)

            // Error banner
            if (this.error.length > 0) {
              Row() {
                Text('⚠ ')
                  .fontSize(14)
                Text(this.error)
                  .fontSize(14)
                  .fontColor('#b91c1c')
              }
              .width('100%')
              .padding(12)
              .backgroundColor('#fef2f2')
              .borderRadius(8)
              .margin({ left: 16, right: 16, bottom: 8 })
            }

            // Metric cards with charts
            Scroll() {
              Column() {
                ForEach(ALL_METRICS, (metric: MetricConfig) => {
                  this.metricChartCard(metric)
                }, (metric: MetricConfig) => metric.key)
              }
              .padding({ left: 16, right: 16, bottom: 24 })
              .width('100%')
            }
            .layoutWeight(1)
            .scrollBar(BarState.Off)
          }
          .width('100%')
          .height('100%')
        }
      }
      .width('100%')
      .height('100%')
      .backgroundColor('#f3f4f6')
    }
    .title('历史趋势')
  }
}
