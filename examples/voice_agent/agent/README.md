## Example Agent

Voice agent built with [LiveKit Agents](https://docs.livekit.io/agents/) to be paired with this example. You can use this as a starting point to build your own agents with hardware interaction capabilities.

## Requirements

- Python 3.9 or later
- [LiveKit Cloud](https://cloud.livekit.io/) Project
- Sandbox Token Server (created from your cloud project)
- API key for OpenAI

## Running locally

1. Creating an *.env* file in this directory containing the following keys:

```sh
LIVEKIT_API_KEY=<your API Key>
LIVEKIT_API_SECRET=<your API Secret>
LIVEKIT_URL=<your server URL>
OPENAI_API_KEY=<your OpenAI API Key>
```

2. Download the required files and run the agent in development mode:

```sh
python agent.py download-files
python agent.py dev
```

With the agent running, it will automatically join the room with the ESP32.
