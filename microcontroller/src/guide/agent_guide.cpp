#include "agent_guide.h"
#include <string.h>
#include <strings.h>

namespace {
	// Placeholder the device address replaces
	constexpr const char *HOST_PLACEHOLDER = "{host}";

	// The guide, with the address as a placeholder (kept in flash)
	constexpr const char *AGENT_GUIDE = R"(# Gizmo Camera

A camera on the local network (an ESP32-CAM with an OV2640 sensor and a white flash LED). Ask it for a snapshot and it takes a new JPEG photo right then.

## Connect over MCP (recommended)

The camera is a Model Context Protocol server, with Streamable HTTP at:

```
http://{host}/mcp
```

With Claude Code:

```sh
claude mcp add --transport http gizmo-camera http://{host}/mcp
```

Tools:

- `take_snapshot`: takes a new photo and returns it as an image you can look at. Arguments, all optional:
  - `resolution`: `QQVGA`, `QVGA`, `CIF`, `VGA`, `SVGA` (default), `XGA`, `HD`, `SXGA`, `UXGA`.
  - `quality`: JPEG quality from 4 to 63, lower is sharper and bigger (default 10).
  - `flash`: `true` to light the scene with the flash LED (default `false`).
- `get_camera_info`: the sensor, its resolutions, the quality range and the flash.

## Plain HTTP

Without MCP, the same camera answers plain requests:

- `GET http://{host}/api/capture` returns a JPEG. The query takes `resolution`, `quality` and `flash` (`1` or `0`), for example `http://{host}/api/capture?resolution=SVGA&flash=1`. Without a resolution, it uses the largest (UXGA, 1600x1200). The `X-Resolution` and `X-Capture-Time-Ms` headers describe the photo.
- `GET http://{host}/api/camera` returns the camera's details as JSON.

```sh
curl -o snapshot.jpg "http://{host}/api/capture?resolution=SVGA"
```

## Tips

- Every call takes a new photo: call again to see what changed.
- SVGA is enough to understand a scene. Use UXGA only to read small details: it is slower and costs more tokens.
- Turn the flash on when the photo comes back dark.
- The camera takes one snapshot at a time. A busy camera answers an error (`camera_busy`): wait a moment and try again.
- Errors of the plain API are JSON: `{ "errorId": "invalid_quality", "errorMessage": "...", "data": null }`.
- The camera only answers on its local network, and without authentication: anyone on that network can see what it sees.
)";

	// Find the position of a media type in an Accept header, ignoring the case (-1 if it isn't there)
	int findMediaType(const char *accept, const char *mediaType) {
		const size_t acceptLength = strlen(accept);
		const size_t mediaTypeLength = strlen(mediaType);
		for (size_t position = 0; position + mediaTypeLength <= acceptLength; position++) {
			if (strncasecmp(accept + position, mediaType, mediaTypeLength) == 0) {
				return static_cast<int>(position);
			}
		}
		return -1;
	}
}

// Write the guide piece by piece, with the address the agent used
void writeAgentGuide(const char *host, AgentGuideWriter write, void *context) {
	const size_t placeholderLength = strlen(HOST_PLACEHOLDER);
	const char *remaining = AGENT_GUIDE;

	// Write the text between placeholders, and the address in their place
	const char *placeholder = strstr(remaining, HOST_PLACEHOLDER);
	while (placeholder != nullptr) {
		write(remaining, static_cast<size_t>(placeholder - remaining), context);
		write(host, strlen(host), context);
		remaining = placeholder + placeholderLength;
		placeholder = strstr(remaining, HOST_PLACEHOLDER);
	}

	// Write the end of the guide
	write(remaining, strlen(remaining), context);
}

// Check whether a client prefers Markdown to HTML from its Accept header
bool isMarkdownPreferred(const char *accept) {
	// No header: a plain request, which gets the web app
	if (accept == nullptr) {
		return false;
	}

	// Markdown must be listed, and come before HTML when both are (the quality values are not weighed)
	const int markdownPosition = findMediaType(accept, "text/markdown");
	const int htmlPosition = findMediaType(accept, "text/html");
	return markdownPosition >= 0 && (htmlPosition < 0 || markdownPosition < htmlPosition);
}