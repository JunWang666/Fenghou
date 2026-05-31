# HarmonyOS Atomic Service Plan

This directory is reserved for the HarmonyOS atomic service that will replace
the current NFC-triggered web view experience with a native ArkUI experience.

## Goal

Build a native HarmonyOS atomic service for on-site device inspection:

1. A phone touches the device NFC tag.
2. The NFC tag opens a link containing the device identifier.
3. HarmonyOS routes the link to the atomic service.
4. The atomic service reads the device identifier from the link.
5. The atomic service calls the existing backend API.
6. The user sees the device status without signing in.

The existing web dashboard remains useful for desktop and management workflows.
The atomic service should focus on fast mobile access from NFC.

## Target User Flow

```text
NFC tag on physical device
  -> https://<domain>/nfc/d/<deviceId>
  -> App Linking or atomic service direct link
  -> HarmonyOS atomic service
  -> Native device live page
  -> Existing backend readings API
```

Recommended NFC payload:

```text
https://<domain>/nfc/d/<deviceId>
```

If public device IDs are sensitive or enumerable, prefer:

```text
https://<domain>/nfc/d/<deviceId>?t=<publicReadToken>
```

The token is not a user login token. It only prevents simple device ID
enumeration and can be printed or encoded into each NFC tag.

## Scope

### Phase 1: Native NFC Live Viewer

Create the minimum useful native atomic service:

- Receive an NFC/App Linking/deep link URL.
- Parse `deviceId` and optional public read token.
- Fetch recent readings for the two existing metrics:
  - `flicker_hazard_count`
  - `sunlight_duration_minutes`
- Display latest values, last update time, and loading/error/empty states.
- Provide manual refresh.
- Keep the UI optimized for a quick field check after touching NFC.

This phase replaces the current web route:

```text
/view/:deviceId
```

### Phase 2: Native Trend Page

Add a trend view equivalent to the web dashboard:

- Time windows: 1h, 6h, 24h, 7d, 30d.
- Fetch historical readings from the existing backend.
- Render simple native charts or a compact data timeline.
- Keep chart implementation lightweight. If native chart support is not ready,
  defer advanced charting instead of embedding the full web dashboard.

This phase replaces the current web route:

```text
/dash/:deviceId
```

### Phase 3: Distribution and Operations

Prepare the service for controlled testing and release:

- Configure AppGallery Connect project and atomic service metadata.
- Configure App Linking or atomic service direct links.
- Run local device debugging through DevEco Studio.
- Run internal testing for the development team.
- Run invited testing for selected phones/users.
- Run pre-release checks before public distribution.
- Add monitoring for API failures and service startup problems.

## Proposed Native Pages

### NfcEntryPage

Responsibilities:

- Receive the launch URL.
- Extract `deviceId`.
- Extract optional public read token.
- Validate required parameters.
- Route to the live page.

Fallback states:

- Missing device ID.
- Unsupported URL format.
- Expired or invalid public read token.

### DeviceLivePage

Responsibilities:

- Show device ID.
- Show latest reading time.
- Show latest metric values.
- Show refresh action.
- Auto-load data on entry.
- Handle loading, empty, error, and retry states.

Suggested display:

```text
Device: <deviceId>
Updated: <timestamp>

Flicker hazard count: <value>
Sunlight duration: <value> min

[Refresh]
```

### DeviceTrendPage

Responsibilities:

- Show historical readings.
- Support time window switching.
- Keep the most important metrics visible without dense desktop UI.

This page is optional for the first milestone.

## Backend Integration

Reuse the current backend instead of migrating data into HarmonyOS services.

Current web API pattern:

```text
GET /dash-api/devices/{deviceId}/readings?sensor=<sensor>&from=<iso>&limit=<n>
```

Native service should use a full HTTPS API base URL:

```text
https://<domain>/dash-api
```

Do not rely on the web app's relative path:

```text
/dash-api
```

Expected first request:

```text
GET https://<domain>/dash-api/devices/{deviceId}/readings
  ?sensor=flicker_hazard_count
  &sensor=sunlight_duration_minutes
  &from=<now-minus-5-minutes>
  &limit=80
```

If public read tokens are used, extend the API with one of these patterns:

```text
GET .../readings?...&publicToken=<token>
```

or:

```text
Authorization: Bearer <publicReadToken>
```

The query parameter approach is easier for NFC links. The header approach avoids
tokens appearing in backend access logs as often, but requires extra native code.

## Testing Plan

### Local Development Testing

Use DevEco Studio with a HarmonyOS phone:

- Run the atomic service directly on a test phone.
- Simulate launch URLs during development.
- Verify API calls against the deployed backend.
- Verify empty, invalid device, weak network, and backend error states.

### NFC Testing

Prepare at least three NFC tags:

- Valid known device.
- Unknown device.
- Malformed or missing device ID.

Verify:

- The phone recognizes the NFC payload.
- The correct atomic service entry opens.
- The correct device ID is displayed.
- Multiple tags do not reuse stale state.
- Weak network errors are understandable and recoverable.

### Controlled Distribution Testing

Use AppGallery Connect testing tracks:

- Internal testing for the development team.
- Invited testing for selected external phones/users.
- Public testing only after the field workflow is stable.

For the requested "specific phones only" workflow, use invited testing with the
Huawei accounts that will be used on those phones. In practice, the control is
usually account-based rather than raw device serial-number based.

### Release Readiness Checks

Before wider release:

- Run pre-release checks.
- Confirm no login is required.
- Confirm public device data is acceptable without identity verification.
- Confirm NFC tags use stable links that will not need to be rewritten.
- Confirm the web fallback URL still works for non-HarmonyOS devices.

## Security Notes

No user authentication is required for the initial product flow. That means the
link is the access boundary.

Minimum recommended protection:

- Do not expose sequential-only access if device data should not be public.
- Use a per-device public read token in the NFC URL.
- Return `404` or a generic error for invalid device/token pairs.
- Rate limit public device reading endpoints.
- Avoid returning unrelated device metadata from the NFC endpoint.

## Open Decisions

- Final production domain for NFC links.
- Whether to use App Linking, atomic service direct links, or both.
- Whether NFC URLs include only `deviceId` or also a public read token.
- Whether Phase 1 includes only the live view or also trend data.
- Whether advanced charts are required in native UI for the first release.
- Which Huawei accounts/phones are included in invited testing.

## Milestone Checklist

- [ ] Create DevEco Studio atomic service project in this directory.
- [ ] Configure package name and signing profile.
- [ ] Configure link routing for NFC/App Linking.
- [ ] Implement `NfcEntryPage`.
- [ ] Implement `DeviceLivePage`.
- [ ] Add backend API client.
- [ ] Add loading, empty, error, and retry states.
- [ ] Test direct launch URL on a local phone.
- [ ] Write NFC tags for test devices.
- [ ] Test NFC launch on at least one HarmonyOS phone.
- [ ] Configure AGC internal testing.
- [ ] Configure AGC invited testing for selected phones/users.
- [ ] Run pre-release checks.
