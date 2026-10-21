// SPDX-FileCopyrightText: 2025 LiveKit, Inc.
//
// SPDX-License-Identifier: Apache-2.0
import {
  type JobContext,
  type JobProcess,
  WorkerOptions,
  cli,
  defineAgent,
  llm,
  voice,
} from '@livekit/agents';
import * as openai from '@livekit/agents-plugin-openai';
import * as silero from '@livekit/agents-plugin-silero';
import * as deepgram from '@livekit/agents-plugin-deepgram';
import * as cartesia from '@livekit/agents-plugin-cartesia';
import { fileURLToPath } from 'node:url';
import { z } from 'zod';
import { log } from 'node:console';

const roomNameSchema = z.enum(['bedroom', 'living room', 'kitchen', 'bathroom', 'office']);

export default defineAgent({
  prewarm: async (proc: JobProcess) => {
    proc.userData.vad = await silero.VAD.load();
  },
  entry: async (ctx: JobContext) => {
    const getWeather = llm.tool({
      description: ' Called when the user asks about the weather.',
      parameters: z.object({
        location: z.string().describe('The location to get the weather for'),
      }),
      execute: async ({ location }) => {
        return `The weather in ${location} is sunny today.`;
      },
    });

    const toggleLight = llm.tool({
      description: 'Called when the user asks to turn on or off the light.',
      parameters: z.object({
        room: roomNameSchema.describe('The room to turn the light in'),
        switchTo: z.enum(['on', 'off']).describe('The state to turn the light to'),
      }),
      execute: async ({ room, switchTo }) => {
        return `The light in the ${room} is now ${switchTo}.`;
      },
    });

    const setLedState = llm.tool({
      description: 'Called when the user asks to turn on or off the LED light.',
      parameters: z.object({
        //color: ledColorSchema.describe('The color of the LED light'),
        colorR: z.number().int().min(0).max(255).describe('The red component of the LED color (0-255)'),
        colorG: z.number().int().min(0).max(255).describe('The green component of the LED color (0-255)'),
        colorB: z.number().int().min(0).max(255).describe('The blue component of the LED color (0-255)'),
        
      }),
      execute: async ({ colorR, colorG, colorB }) => {
        log(`Setting LED color to R: ${colorR}, G: ${colorG}, B: ${colorB}`);
        
        const remoteParticipant = ctx.room.remoteParticipants.values().next().value;
        if (!remoteParticipant) {
          return 'No remote participant found to set the LED color.';
        }
        
        const result = await ctx.room.localParticipant!.performRpc({
            destinationIdentity: remoteParticipant.identity!,
            method: 'set_led_color',
            payload: [colorR, colorG, colorB].join(',') 
          })

          return `The LED color has been changed.`;
          // Simulate setting the LED state
          //if (response.success) {
            return `The LED color has been changed.`;
          //} else {
            //return 'Failed to set the LED state. Please try again later.';
          //}
      },
    });

    const agent = new voice.Agent({
      instructions:
        "You are a helpful assistant created by LiveKit, always speaking English, you can hear the user's message and respond to it.",
      tools: {
        getWeather,
        toggleLight,
        setLedState,
      },
    });

    const session = new voice.AgentSession({
      llm: new openai.realtime.RealtimeModel(),
      stt: new deepgram.STT({
        model: 'nova-2-general',
        language: 'multi',
      }),
      //llm: new openai.LLM({
      //  model: 'gpt-4o-mini',
      //}),
      tts: new cartesia.TTS({
        model: 'sonic-english',
        voice: 'c99d36f3-5ffd-4253-803a-535c1bc9c306',
      }),
      // enable to allow chaining of tool calls
      voiceOptions: {
        maxToolSteps: 5,
      },
    });

    await session.start({
      agent,
      room: ctx.room,
    });

    // join the room when agent is ready
    await ctx.connect();
  },
});

cli.runApp(new WorkerOptions({ agent: fileURLToPath(import.meta.url) }));
