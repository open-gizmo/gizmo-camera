#ifndef AGENT_GUIDE_H
#define AGENT_GUIDE_H

#include <stddef.h>

// The guide for AI agents (Markdown, at /llms.txt and at / for clients that ask for Markdown), kept free of hardware headers so the native tests can use it

// Receive a piece of the guide
using AgentGuideWriter = void (*)(const char *text, size_t length, void *context);

// Write the guide piece by piece, with the address the agent used to reach the device (for example "gizmo-camera.local" or "192.168.1.42")
void writeAgentGuide(const char *host, AgentGuideWriter write, void *context);

// Check whether a client prefers Markdown to HTML from its Accept header (text/markdown listed, and before text/html if both are)
bool isMarkdownPreferred(const char *accept);

#endif