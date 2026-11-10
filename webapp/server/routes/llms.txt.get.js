import { setResponseHeader } from 'h3';

// Stand in for the guide for AI agents, which the firmware writes with its own address (agent_guide.cpp)
export default defineEventHandler(event => {
    setResponseHeader(event, 'Content-Type', 'text/markdown; charset=utf-8');
    return '# Gizmo Camera\n\nThis is the development server. The real guide for AI agents is served by the firmware at /llms.txt, with the address of the device.\n';
});