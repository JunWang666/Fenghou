import { hilog } from '@kit.PerformanceAnalysisKit';
import { fetchReadings } from '../api';
import type { Reading, MetricConfig } from '../models';
import { ALL_METRICS, VIEW_WINDOW_MINUTES, formatMetricTime, formatValue } from '../models';

const DOMAIN = 0x0000;
const TAG = 'DeviceLivePage';

interface LatestReading {
  value: number | null;
  time: string;
}

@Component
export struct DeviceLivePage {
  @Prop deviceId: string = '';
  @State loading: boolean = true;
  @State error: string = '';
  @State latestMap: Map<string, LatestReading> = new Map();
  @State loadedAt: string = '';
  pathStack: NavPathStack = new NavPathStack();

  aboutToAppear(): void {
    this.loadData();
  }

  private async loadData(): Promise<void> {
    this.loading = true;
    this.error = '';

    const now: number = Date.now();
    const from: string = new Date(now - VIEW_WINDOW_MINUTES * 60 * 1000).toISOString();
    const sensorKeys: string[] = ALL_METRICS.map((m: MetricConfig) => m.key);

    try {
      const response = await fetchReadings(this.deviceId, sensorKeys, from, 80);
      const newMap = new Map<string, LatestReading>();

      for (const metric of ALL_METRICS) {
        newMap.set(metric.key, { value: null, time: '' });
      }

      for (const reading of response.readings) {
        const existing = newMap.get(reading.sensor_name);
        if (!existing || existing.time.length === 0 || reading.time > existing.time) {
          newMap.set(reading.sensor_name, {
            value: reading.value_number,
            time: reading.time
          });
        }
      }

      this.latestMap = newMap;
      this.loadedAt = new Date().toISOString();
      hilog.info(DOMAIN, TAG, `Loaded ${response.readings.length} readings`);
    } catch (err) {
      this.error = err instanceof Error ? err.message : '数据加载失败';
      hilog.error(DOMAIN, TAG, `Load error: ${this.error}`);
    } finally {
      this.loading = false;
    }
  }

  private formatCardTime(isoTime: string): string {
    if (isoTime.length === 0) {
      return '暂无最近数据';
    }
    return formatMetricTime(isoTime);
  }

  private getLatestTime(key: string): string {
    const reading = this.latestMap.get(key);
    return reading ? reading.time : '';
  }

  private formatCardValue(metric: MetricConfig): string {
    const reading = this.latestMap.get(metric.key);
    if (!reading) {
      return '--';
    }
    return formatValue(reading.value, metric.precision, metric.unit);
  }

  @Builder
  metricCard(metric: MetricConfig) {
    Column() {
      Row() {
        Text(metric.icon)
          .fontSize(18)
          .margin({ right: 6 })
        Text(metric.label)
          .fontSize(14)
          .fontColor('#374151')
      }
      .width('100%')
      .margin({ bottom: 8 })

      Text(this.formatCardValue(metric))
        .fontSize(28)
        .fontWeight(FontWeight.Bold)
        .fontColor(metric.color)
        .width('100%')
        .margin({ bottom: 6 })

      Text(this.formatCardTime(this.getLatestTime(metric.key)))
        .fontSize(12)
        .fontColor('#9ca3af')
        .width('100%')
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
        if (this.loading && this.latestMap.size === 0) {
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
        } else if (this.error.length > 0 && this.latestMap.size === 0) {
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
          Scroll() {
            Column() {
              // Header
              Column() {
                Text('实时查看')
                  .fontSize(12)
                  .fontColor('#6b7280')
                  .margin({ bottom: 4 })
                Text(this.deviceId)
                  .fontSize(22)
                  .fontWeight(FontWeight.Bold)
                  .fontColor('#111827')
                  .margin({ bottom: 4 })
                Text(`最近 ${VIEW_WINDOW_MINUTES} 分钟内的最新读数`)
                  .fontSize(13)
                  .fontColor('#6b7280')
              }
              .width('100%')
              .padding({ left: 16, right: 16, top: 16, bottom: 12 })

              // Error banner (non-fatal)
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
                .margin({ left: 16, right: 16, bottom: 12 })
              }

              // Metric cards grid
              GridRow({ columns: { sm: 2, md: 3, lg: 4 }, gutter: { x: 12, y: 12 } }) {
                ForEach(ALL_METRICS, (metric: MetricConfig) => {
                  GridCol() {
                    this.metricCard(metric)
                  }
                }, (metric: MetricConfig) => metric.key)
              }
              .padding({ left: 16, right: 16 })

              // Action row
              Row() {
                Button('查看历史趋势')
                  .type(ButtonType.Capsule)
                  .fontSize(13)
                  .height(36)
                  .backgroundColor('#0f766e')
                  .onClick(() => {
                    this.pathStack.pushPathByName('DeviceTrendPage', null);
                  })
                Blank()
              }
              .width('100%')
              .padding({ left: 16, right: 16, top: 12 })

              // Status bar
              Row() {
                Text(this.loadedAt.length > 0 ? `更新于 ${formatMetricTime(this.loadedAt)}` : '等待数据')
                  .fontSize(13)
                  .fontColor('#6b7280')
                  .layoutWeight(1)
                Button(this.loading ? '加载中...' : '刷新')
                  .type(ButtonType.Capsule)
                  .fontSize(13)
                  .width(80)
                  .height(36)
                  .backgroundColor('#2563eb')
                  .enabled(!this.loading)
                  .onClick(() => {
                    this.loadData();
                  })
              }
              .width('100%')
              .padding({ left: 16, right: 16, top: 16, bottom: 24 })
              .alignItems(VerticalAlign.Center)
            }
          }
          .width('100%')
          .height('100%')
          .scrollBar(BarState.Off)
        }
      }
      .width('100%')
      .height('100%')
      .backgroundColor('#f3f4f6')
    }
    .title('设备实时数据')
  }
}
