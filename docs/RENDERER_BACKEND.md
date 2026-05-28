# Cesium Native Renderer Backend

This repository is an Android host backend for `cesium-native`.

## Ownership Boundary

`cesium-native` owns:

- tileset lifecycle and traversal
- screen-space-error based tile selection
- tile load queues and cache eviction
- raster overlay provider selection
- raster-to-geometry attachment, fallback, translation, scale, and overlay texture coordinate IDs

This repository owns:

- Android view and EGL lifecycle
- conversion from host camera state to `Cesium3DTilesSelection::ViewState`
- `IPrepareRendererResources` implementation
- GPU buffer and texture upload
- OpenGL drawing of tiles returned by `cesium-native`
- Android and Flutter API adapters

## Renderer Contract

The native bridge must follow this loop:

1. Build a `ViewState` from the current WGS84 camera.
2. Call `Tileset::updateViewGroup`.
3. Call `Tileset::loadTiles`.
4. Dispatch Cesium main-thread tasks on the render thread.
5. Draw only `ViewUpdateResult::tilesToRenderThisFrame`.

The host must not implement its own tile selection, imagery tile selection,
parent fallback, or raster attachment lifecycle.

During active gestures the backend may temporarily raise screen-space error,
reduce concurrent loads, and throttle render-thread main-task dispatch. This is
still Cesium Native tile management: the host only reports whether the camera is
moving, while the backend decides how much tile and upload pressure to allow.

Geometry and raster GPU uploads are also backend-scheduled. Cesium Native may
request render resources and raster attachments, but this renderer uploads VBOs,
IBOs, and raster textures across frames according to camera motion and frame
pressure, then replays pending raster attachments when textures become ready.

The renderer derives projection clipping planes from WGS84 camera altitude and
horizon distance instead of using a fixed planet-scale far plane. This preserves
more depth precision for low and mid altitude interaction.

## Camera Contract

The public camera is geodetic WGS84 state:

- `longitude` and `latitude` are degrees.
- `altitudeMeters` is height above the WGS84 ellipsoid.
- `bearing` and `pitch` describe the host view orientation.

The SDK must not expose WebMercator-style zoom levels as camera state. Gesture
pinch and double-tap interactions change `altitudeMeters` multiplicatively,
then the native bridge passes that altitude directly into
`CesiumGeospatial::Cartographic` and `Cesium3DTilesSelection::ViewState`.

Host gestures are forwarded to the native renderer backend as screen-space
events. The C++ backend owns WGS84 ellipsoid ray picking, focus-stable pan and
zoom, rotate, tilt, auto-orbit, and camera sanitization. Android and Flutter
hosts must not duplicate WGS84 camera math, tile selection, imagery selection,
or render fallback logic.

## Public Scope

The current supported imagery input is one URL-template raster overlay. It maps
to `CesiumRasterOverlays::UrlTemplateRasterOverlay`.

Raster geometry must use Cesium Native generated `_CESIUMOVERLAY_*` texture
coordinates. The renderer must not synthesize fallback UVs from linear
longitude/latitude interpolation because WebMercator imagery such as Gaode
satellite tiles will visibly mis-stitch on the ellipsoid.

Fields that are not implemented in the Cesium Native backend should not be
exposed as committed API.

The default development template is an HTTP tile endpoint so the renderer can
be validated without disabling TLS certificate verification. Production
providers must use their own licensing, authentication, and transport policy.

## Build Compatibility

The native bridge must be compiled with ABI settings that match the linked
Cesium Native Android package. The default local package is a release build, so
the bridge defines `NDEBUG` for compatibility with Cesium Native inline header
types such as `ReferenceCounted`.

To run without `NDEBUG`, first build and link a matching debug Cesium Native
Android package.
