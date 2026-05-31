import { hilog } from '@kit.PerformanceAnalysisKit';
import uri from '@ohos.uri';
import { DeviceLivePage } from './DeviceLivePage';
import { DeviceTrendPage } from './DeviceTrendPage';

const DOMAIN = 0x0000;
const TAG = 'Index';

@Entry
@Component
struct Index {
  @Provide('pathStack') pathStack: NavPathStack = new NavPathStack();
  @State deviceId: string = '';
  @State errorMessage: string = '';
  @State resolved: boolean = false;

  aboutToAppear(): void {
    AppStorage.setOrCreate<string>('deviceId', '');
    const launchUri: string = AppStorage.get<string>('launchUri') ?? '';
    hilog.info(DOMAIN, TAG, `launchUri from AppStorage: ${launchUri}`);
    if (launchUri.length > 0) {
      this.extractDeviceId(launchUri);
    }
  }

  private extractDeviceId(rawUri: string): void {
    try {
      hilog.info(DOMAIN, TAG, `Parsing URI: ${rawUri}`);
      const parsedUri: uri.URI = new uri.URI(rawUri);
      const path: string = parsedUri.path ?? '';
      const pathParts: string[] = path.split('/').filter((part: string) => part.length > 0);

      let id: string = '';
      // Pattern: /view/<deviceId> or /dash/<deviceId>
      if (pathParts.length >= 2) {
        const mode: string = pathParts[0];
        if (mode === 'view' || mode === 'dash') {
          id = decodeURIComponent(pathParts[1]);
        }
      }
      // Pattern: /nfc/d/<deviceId>
      if (pathParts.length >= 3 && pathParts[0] === 'nfc' && pathParts[1] === 'd') {
        id = decodeURIComponent(pathParts[2]);
      }

      if (id.length > 0) {
        this.deviceId = id;
        this.resolved = true;
        this.errorMessage = '';
        hilog.info(DOMAIN, TAG, `Device ID: ${id}`);
        this.pathStack.pushPathByName('DeviceLivePage', null);
      } else {
        this.errorMessage = '无法从链接中解析设备ID';
        hilog.warn(DOMAIN, TAG, `No device ID found in URI: ${rawUri}`);
      }
    } catch (err) {
      this.errorMessage = '链接格式无效';
      hilog.error(DOMAIN, TAG, `URI parse error: ${err}`);
    }
  }

  build() {
    Navigation(this.pathStack) {
      Column() {
        if (this.errorMessage.length > 0) {
          Column() {
            Text('⚠')
              .fontSize(48)
              .margin({ bottom: 16 })
            Text(this.errorMessage)
              .fontSize(16)
              .fontColor('#666666')
              .textAlign(TextAlign.Center)
            Text('请通过NFC标签或分享链接打开')
              .fontSize(14)
              .fontColor('#999999')
              .margin({ top: 8 })
              .textAlign(TextAlign.Center)
          }
          .width('100%')
          .height('100%')
          .justifyContent(FlexAlign.Center)
        } else if (!this.resolved) {
          Column() {
            Text('📱')
              .fontSize(48)
              .margin({ bottom: 16 })
            Text('等待启动')
              .fontSize(16)
              .fontColor('#666666')
              .textAlign(TextAlign.Center)
            Text('请通过NFC标签或分享链接打开')
              .fontSize(14)
              .fontColor('#999999')
              .margin({ top: 8 })
              .textAlign(TextAlign.Center)
          }
          .width('100%')
          .height('100%')
          .justifyContent(FlexAlign.Center)
        } else {
          Column()
            .width('100%')
            .height('100%')
        }
      }
      .width('100%')
      .height('100%')
      .backgroundColor('#f3f4f6')
    }
    .title(this.resolved ? `凤候-${this.deviceId}` : '凤候')
    .navDestination(this.pageMap)
  }

  @Builder
  pageMap(name: string, param: Object) {
    if (name === 'DeviceLivePage') {
      DeviceLivePage({ deviceId: this.deviceId })
    } else if (name === 'DeviceTrendPage') {
      DeviceTrendPage({ deviceId: this.deviceId })
    }
  }
}
