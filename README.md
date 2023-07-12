# ANSI C ChatGPT client

This is a very simple ChatGPT client written in ANSI C (C89) for UNIX-based systems only (for now). It communicates using OpenAI's API to provide a conversational-style terminal interface.

You need to be registered with OpenAI and obtain an API key (https://platform.openai.com/account/api-keys). Be aware that API usage may be billed.

Your OpenAI API key needs to be written in the `~/.openaikey` file, like this: `echo OPEN_AI_KEY > ~/.openaikey`, replacing `OPEN_AI_KEY` with your API key. In conversation mode, you can also input it with the `/apikey` shell command.

You have two ways to interact with the client:
- Directly from the shell, without initiating a conversation. For example, you can ask:

  `$ chatgpt What\'s the APT command used for in Linux?` (don't forget to escape the necessary characters)

  And it will reply, for example:

  `The 'apt' command in Linux is primarily used for package management. [...] 'apt' is an abbreviation for Advanced Package Tool.`
- Initiating a conversation. You can do it by executing the program with no arguments (for example, `chatgpt`). An example conversation would look like:

  ```
  $ chatgpt
  ChatGPT conversation shell. Type /help for command usage.
  
  gpt-3.5-turbo> /model gpt-3.5-turbo-16k
  Model successfully set/changed.
  gpt-3.5-turbo-16k> /system Act like a pirate
  System prompt successfully set/changed.
  gpt-3.5-turbo-16k> Hi!
  Arr! Ahoy matey! How can I be helpin' ye today?
  
  -- Used 34 tokens (in total) --
  gpt-3.5-turbo-16k> What are you doing?
  I be a trusty pirate, sailin' the digital seas! Tellin' tales, sharin' knowledge, and assistin' ye with any tasks ye be needin'. What can I do for ye, me heartie?
  
  -- Used 129 tokens (in total) --
  gpt-3.5-turbo-16k> /exit
  Bye!
  ```

  Shell commands (the ones starting with `/`) are not sent to the language model. You can view all available shell commands by typing `/help`. Shell command autocomplete is also available by pressing the `TAB` key.

## Installing ChatGPT client

Due to [dependency hell](https://en.wikipedia.org/wiki/Dependency_hell), I will not provide builds of this tool for now. I may provide them if the client gets ported to Windows. So, if you want to use it, you'll need to build it yourself.

## Building the client

You will need to have the `libcurl4` development libraries installed with SSL and HTTP/2 support. Also, you will require to have the `cJSON` and `readline` development libraries too.

After that, you can build the tool with just one command:

```
$ gcc -o chatgpt chatgpt.c -O2 -lcurl -lcjson -lreadline
```

Clang is also supported.

## Contributing to the development

You can contribute to the development by submitting a pull request. An orientative to-do list is available below.

The ANSI C ChatGPT client is licensed using the GPL-3.0 license, more information in the LICENSE file.

### To-Do

- [x] <s>Main ANSI C client</s>
- [x] <s>Conversation mode</s>
- [ ] Windows port
- [ ] Changing default model in configuration
- [ ] Securely store API key
- [ ] Maintaining conversations between sessions

<br>

**&copy; 2023, Lumito - [www.lumito.net](https://www.lumito.net)**
