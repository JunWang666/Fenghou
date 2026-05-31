import { http } from '@kit.NetworkKit';
import { hilog } from '@kit.PerformanceAnalysisKit';
import type { ReadingsResponse } from './models';

const DOMAIN = 0x0000;
const TAG = 'FenghouAPI';
const API_BASE = 'https://fenghou.goudaijun.top/dash-api';

async function getReadingsJson(url: string): Promise<ReadingsResponse> {
  const httpRequest = http.createHttp();
  try {
    hilog.info(DOMAIN, TAG, `GET ${url}`);
    const response = await httpRequest.request(url, {
      method: http.RequestMethod.GET,
      header: { 'Content-Type': 'application/json' },
      connectTimeout: 15000,
      readTimeout: 15000
    });

    if (response.responseCode === http.ResponseCode.OK) {
      const result: string = response.result as string;
      const parsed: ReadingsResponse = JSON.parse(result) as ReadingsResponse;
      if (!parsed.ok) {
        throw new Error('请求失败');
      }
      return parsed;
    }
    throw new Error(`HTTP ${response.responseCode}`);
  } catch (err) {
    if (err instanceof Error) {
      hilog.error(DOMAIN, TAG, `Request error: ${err.message}`);
      throw err;
    }
    throw new Error('网络请求失败');
  } finally {
    httpRequest.destroy();
  }
}

export function fetchReadings(deviceId: string, sensors: string[], from: string, limit: number): Promise<ReadingsResponse> {
  let queryParts: string[] = [];
  for (const sensor of sensors) {
    queryParts.push(`sensor=${encodeURIComponent(sensor)}`);
  }
  queryParts.push(`from=${encodeURIComponent(from)}`);
  queryParts.push(`limit=${limit.toString()}`);

  const url: string = `${API_BASE}/devices/${encodeURIComponent(deviceId)}/readings?${queryParts.join('&')}`;
  return getReadingsJson(url);
}
