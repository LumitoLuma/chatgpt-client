/*
    A simple ChatGPT client written in C
    Copyright (C) 2023 Lumito - www.lumito.net

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <cjson/cJSON.h>
#include <curl/curl.h>
#include <pwd.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define APP_VERSION "0.5.1"

unsigned int tokens = 0;

struct string
{
    char *ptr;
    size_t len;
};

typedef enum
{
    false,
    true
} bool;

void init_string(struct string *__str)
{
    __str->len = 0;
    __str->ptr = malloc(__str->len + 1);
    if (__str->ptr == NULL)
    {
        fprintf(stderr, "malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    __str->ptr[0] = '\0';
}

char *strdup(const char *s)
{
    char *d = malloc(strlen(s) + 1);
    if (d == NULL)
        return NULL;
    strcpy(d, s);
    return d;
}

char *concat(const char *__str1, const char *__str2)
{
    char *result = malloc(strlen(__str1) + strlen(__str2) + 1);

    if (result == NULL)
    {
        free(result);
        return "";
    }

    strcpy(result, __str1);
    strcat(result, __str2);
    return result;
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL)
    {
        fprintf(stderr, "realloc() failed\n");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;

    return size * nmemb;
}

unsigned short contains_str_before_space(const char *full_str, const char *coincidence, char **remaining_data)
{
    const char *space = strchr(full_str, ' ');
    const char *found_coinc = strstr(full_str, coincidence);

    if (found_coinc == NULL || (space != NULL && found_coinc + strlen(coincidence) != space))
    {
        return 0;
    }

    if (found_coinc < space)
    {
        if (remaining_data != NULL)
        {
            *remaining_data = space ? (char *)(space + 1) : NULL;
        }
        return 1;
    }

    if (space == NULL)
    {
        if (strcmp(full_str, coincidence) == 0)
            return 1;
        return 0;
    }

    return 0;
}

char *chatgpt_curl_perform(const char *data, const char *apikey, const char *endpoint)
{
    CURL *curl;
    CURLcode res;

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();

    if (!curl)
    {
        curl_global_cleanup();
        fprintf(stderr, "Error: Could not initialize cURL\n");
        return NULL;
    }

    struct curl_slist *headers = NULL;
    char *auth_header = concat("Authorization: Bearer ", apikey);
    curl_easy_setopt(curl, CURLOPT_URL, endpoint);

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    struct string json_output;
    init_string(&json_output);

    curl_easy_setopt(curl, CURLOPT_POST, 1L);

    size_t i = 1;

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(data));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_output);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);

    res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        fprintf(stderr, "HTTP request failed: %s.", curl_easy_strerror(res));
        if (strcmp(curl_easy_strerror(res), "Timeout was reached") == 0)
        {
            fprintf(stderr, " This is probably OpenAI's fault. Try again later.");
        }
        printf("\n");
        return NULL;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    cJSON *root = cJSON_Parse(json_output.ptr);
    if (!root)
    {
        fprintf(stderr, "Error parsing result. Check your API key, network connection, account credits and model used. API response:\n%s", json_output.ptr);
        return NULL;
    }
    cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (!cJSON_IsArray(choices))
    {
        fprintf(stderr, "Error parsing result. Check your API key, network connection, account credits and model used. API response:\n%s", json_output.ptr);
        return NULL;
    }
    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    if (!cJSON_IsObject(choice))
    {
        fprintf(stderr, "Error parsing result. Check your API key, network connection, account credits and model used. API response:\n%s", json_output.ptr);
        return NULL;
    }
    cJSON *message = cJSON_GetObjectItemCaseSensitive(choice, "message");
    if (!cJSON_IsObject(message))
    {
        fprintf(stderr, "Error parsing result. Check your API key, network connection, account credits and model used. API response:\n%s", json_output.ptr);
        return NULL;
    }
    cJSON *content = cJSON_GetObjectItemCaseSensitive(message, "content");
    if (!cJSON_IsString(content))
    {
        fprintf(stderr, "Error parsing result. Check your API key, network connection, account credits and model used.\n\nAPI response:\n%s", json_output.ptr);
        return NULL;
    }

    for (i = 0; i < 2; i++)
    {
        if (content->valuestring[0] == '\n')
            content->valuestring++;
        else
            break;
    }

    char *curl_result = malloc(strlen(content->valuestring) + 1);
    strcpy(curl_result, content->valuestring);

    for (; i > 0; i--)
        content->valuestring--;

    cJSON *usage = cJSON_GetObjectItemCaseSensitive(root, "usage");
    if (!cJSON_IsObject(usage))
    {
        fprintf(stderr, "Error parsing result. Token usage is not available, but should be. API response:\n%s", json_output.ptr);
        return NULL;
    }

    cJSON *totalusage = cJSON_GetObjectItemCaseSensitive(usage, "total_tokens");
    if (!cJSON_IsNumber(totalusage))
    {
        fprintf(stderr, "Error parsing result. Total tokens aren't available, but they should. API response:\n%s", json_output.ptr);
        return NULL;
    }

    if (cJSON_IsNumber(totalusage))
    {
        tokens += totalusage->valueint;
    }
    else
        fprintf(stderr, "Error parsing result. \"total_tokens\"'s value is not a number. API response:\n%s", json_output.ptr);

    cJSON_Delete(root);

    free(auth_header);

    curl_global_cleanup();

    return curl_result;
}

char *escape_string(const char *str)
{
    size_t new_len = strlen(str), i = 0;
    for (; i < strlen(str); i++)
        if (str[i] == '\"' || str[i] == '\\' || str[i] == '\n' || str[i] == '\r' || str[i] == '\t')
            new_len++;

    char *escaped_str = (char *)malloc(new_len + 1);
    size_t j = 0;
    for (i = 0; i < strlen(str); i++)
    {
        switch (str[i])
        {
        case '\"':
            escaped_str[j++] = '\\';
            escaped_str[j++] = '\"';
            break;
        case '\\':
            escaped_str[j++] = '\\';
            escaped_str[j++] = '\\';
            break;
        case '\n':
            escaped_str[j++] = '\\';
            escaped_str[j++] = 'n';
            break;
        case '\r':
            escaped_str[j++] = '\\';
            escaped_str[j++] = 'r';
            break;
        case '\t':
            escaped_str[j++] = '\\';
            escaped_str[j++] = 't';
            break;
        default:
            escaped_str[j++] = str[i];
            break;
        }
    }
    escaped_str[j] = '\0';

    return escaped_str;
}

char *autocomplete(const char *text, int state)
{
    /* Not the best code at all, but it requires few memory management */
    if (strlen(text) < 1)
        return NULL;
    const char *available_commands[9] = {"/apikey", "/endpoint", "/exit",
                                         "/help", "/model", "/reset",
                                         "/showusage", "/system", "/version"};
    unsigned short total_commands = 0;
    size_t text_length = strlen(text);

    unsigned short i = 0;
    for (; i < 9; i++)
    {
        if (strncmp(available_commands[i], text, text_length) == 0)
            total_commands++;
    }
    if (total_commands == 0)
        printf("\a");
    else if (total_commands == 1)
    {
        for (i = 0; i < 9; i++)
            if (strncmp(available_commands[i], text, text_length) == 0)
            {
                rl_replace_line("", 0);
                rl_redisplay();
                rl_insert_text(available_commands[i]);
                break;
            }
    }
    else
    {
        printf("\n");
        for (i = 0; i < 9; i++)
            if (strncmp(available_commands[i], text, text_length) == 0)
                printf("%s\n", available_commands[i]);
        rl_on_new_line();
        rl_redisplay();
        printf("\a");
    }

    return NULL;
}

char **custom_completion(const char *text, int start, int end)
{
    rl_attempted_completion_over = 1;

    return rl_completion_matches(text, autocomplete);
}

/* Handle Ctrl+C presses in shell mode (else will quit the program) */
void ctrlCHandler(int sig_num)
{
    signal(SIGINT, ctrlCHandler);
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int shell_mode(char *apikey, char *def_model)
{
    signal(SIGINT, ctrlCHandler);
    char *orig_apikey = NULL, *endpoint = "https://api.openai.com/v1/chat/completions";
    if (apikey != NULL)
    {
        orig_apikey = apikey;
    }
    printf("ChatGPT conversation shell. Type /help for command usage.\n\n");
    char *prompt_1 = "{\"model\": \"";
    char *model = def_model;
    char *prompt_2 = "\", \"temperature\": ";
    float temperature = 1.0F;
    char *prompt_3 = ", \"messages\": [";
    char *prompt_system = "";
    char *conversation = "";
    char *prompt_4 = "]}";

    bool show_usage = true;

    while (true)
    {
        char *read_result;

        rl_attempted_completion_function = custom_completion;
        rl_variable_bind("bell-style", "none");

        while ((read_result = readline(concat(model, "> "))) != NULL)
        {
            if (strlen(read_result) > 0)
                add_history(read_result);

            size_t len = strlen(read_result);
            if (len > 0 && read_result[len - 1] == '\n')
                read_result[len - 1] = '\0';

            if (read_result[0] == '/')
            {
                char *remaining_data = NULL;
                if (contains_str_before_space(read_result, "/help", &remaining_data))
                {
                    printf("Commands starting with / are shell commands, else they are sent to the model.\n\n");
                    printf("Available shell commands:\n");
                    printf("  /system <prompt>        - Change the \"system\" prompt, run /system with no prompt to clear.\n");
                    printf("  /model <model>          - Change the model used, run /model with no model to reset.\n");
                    printf("  /apikey <key>           - Change or set the API key used, run /apikey with no key to reset.\n");
                    printf("  /showusage <true|false> - Show used tokens during conversation. Run with no value to reset.\n");
                    printf("  /temperature <value>    - Change model's temperature. <value> must be or be between 0.0 and 2.0. Run with no value to reset.\n");
                    printf("  /reset                  - Reset the conversation.\n");
                    printf("  /import <file>          - Import conversation from <file>.\n");
                    printf("  /export <file>          - Export current conversation to <file>.\n");
                    printf("  /endpoint <URL>         - (EXPERT ONLY) Change the API endpoint used, run /endpoint with no URL to reset.\n");
                    printf("  /clear                  - Clear terminal screen.\n");
                    printf("  /help                   - Show this help message.\n");
                    printf("  /version                - Show application version.\n");
                    printf("  /exit                   - Exit the shell.\n");
                }
                else if (contains_str_before_space(read_result, "/system", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        prompt_system = "";
                        printf("System prompt successfully cleared.\n");
                        continue;
                    }
                    prompt_system = concat("{\"role\": \"system\", \"content\": \"", escape_string(remaining_data));
                    prompt_system = concat(prompt_system, "\"},");
                    printf("System prompt successfully set/changed.\n");
                }
                else if (contains_str_before_space(read_result, "/model", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        model = def_model;
                        printf("Model successfully reset (set to %s).\n", model);
                        continue;
                    }
                    model = remaining_data;
                    printf("Model successfully set/changed.\n");
                }
                else if (contains_str_before_space(read_result, "/apikey", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        apikey = orig_apikey;
                        printf("API key successfully reset.\n");
                        continue;
                    }
                    apikey = remaining_data;
                    printf("API key successfully set/changed.\n");
                }
                else if (contains_str_before_space(read_result, "/endpoint", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        endpoint = "https://api.openai.com/v1/chat/completions";
                        printf("API endpoint successfully reset.\n");
                        continue;
                    }
                    endpoint = remaining_data;
                    printf("API endpoint successfully set/changed.\n");
                }
                else if (contains_str_before_space(read_result, "/showusage", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        show_usage = true;
                        printf("Show usage successfully reset (set to %s).\n", show_usage ? "true" : "false");
                        continue;
                    }
                    if (strcmp(remaining_data, "false") == 0 || strcmp(remaining_data, "0") == 0)
                    {
                        show_usage = false;
                        printf("Show usage set to false.\n");
                    }
                    else if (strcmp(remaining_data, "true") == 0 || strcmp(remaining_data, "1") == 0)
                    {
                        show_usage = true;
                        printf("Show usage set to true.\n");
                    }
                    else
                        printf("Show usage value not recognized.\n");
                }
                else if (contains_str_before_space(read_result, "/temperature", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        temperature = 1.0F;
                        printf("Temperature successfully reset (set to %.1f).\n", temperature);
                        continue;
                    }
                    if (0.0F <= atof(remaining_data) && atof(remaining_data) <= 2.0F)
                    {
                        temperature = atof(remaining_data);
                        printf("Temperature set to %.1f.\n", temperature);
                    }
                    else
                        printf("Temperature invalid. Must be greater or equal to 0 and less or equal to 2.\n");
                }
                else if (contains_str_before_space(read_result, "/reset", &remaining_data))
                {
                    conversation = "";
                    prompt_system = "";
                    printf("Conversation successfully reset.\n");
                }
                else if (contains_str_before_space(read_result, "/version", &remaining_data))
                    printf("%s\n", APP_VERSION);
                else if (contains_str_before_space(read_result, "/clear", &remaining_data))
                {
                    rl_clear_display(0, 0);
                    printf("\r");
                    rl_replace_line("", 0);
                    rl_redisplay();
                }
                else if (contains_str_before_space(read_result, "/export", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        printf("No destination file provided. Aborting.\n");
                        continue;
                    }
                    FILE *fp = fopen(remaining_data, "w");
                    if (fp == NULL)
                    {
                        printf("Error while opening file for writing. Aborting.\n");
                        continue;
                    }
                    fprintf(fp, "MODEL %s\nTEMP %.1f\nSYS %s\nCONV %s\n", model, temperature, prompt_system, conversation);
                    fclose(fp);
                }
                else if (contains_str_before_space(read_result, "/import", &remaining_data))
                {
                    if (remaining_data == NULL)
                    {
                        printf("No source file provided. Aborting.\n");
                        continue;
                    }
                    FILE *fp = fopen(remaining_data, "r");
                    if (fp == NULL)
                    {
                        printf("Error while opening file for reading. Aborting.\n");
                        continue;
                    }
                    char* output = "";
                    int f;
                    while ((f = fgetc(fp)) != EOF)
                    {
                        if (f != '\r')
                        {
                            char str[2];
                            str[0] = f;
                            str[1] = '\0';
                            output = concat(output, str);
                        }
                    }
                    char *ptr1, *token = strtok_r(output, "\n", &ptr1);
                    while (token != NULL)
                    {
                        char* remdata = NULL;
                        if (contains_str_before_space(token, "MODEL", &remdata))
                        {
                            if (remdata != NULL)
                                model = remdata;
                        }
                        else if (contains_str_before_space(token, "TEMP", &remdata))
                        {
                            if (remdata != NULL)
                                temperature = atof(remdata);
                        }
                        else if (contains_str_before_space(token, "SYS", &remdata))
                        {
                            if (remdata != NULL)
                                prompt_system = remdata;
                        }
                        else if (contains_str_before_space(token, "CONV", &remdata))
                        {
                            if (remdata != NULL)
                                conversation = remdata;
                        }
                        token = strtok_r(NULL, "\n", &ptr1);
                    }
                    fclose(fp);
                }
                else if (contains_str_before_space(read_result, "/exit", &remaining_data))
                {
                    printf("Bye!\n");
                    return 0;
                }
                else
                {
                    fprintf(stderr, "Unknown command. Type /help for command usage.\n");
                }
            }
            else if ((read_result[0] != '\0' || read_result == NULL) && apikey != NULL)
            {
                char *prev_conversation = "";
                prev_conversation = concat(prev_conversation, conversation);
                if (strcmp(conversation, "") != 0)
                    conversation = concat(conversation, ",");
                conversation = concat(conversation, "{\"role\": \"user\", \"content\": \"");
                conversation = concat(conversation, escape_string(read_result));
                conversation = concat(conversation, "\"}");

                char *data = (char *)malloc(strlen(prompt_1) + 1);
                strcpy(data, prompt_1);
                data = concat(data, model);
                data = concat(data, prompt_2);
                int f_temp_length = snprintf(NULL, 0, "%.1f", temperature) + 1;
                char *f_temp = malloc(f_temp_length);
                snprintf(f_temp, f_temp_length, "%.1f", temperature);
                data = concat(data, f_temp);
                free(f_temp);
                data = concat(data, prompt_3);
                if (strcmp(prompt_system, "") != 0)
                    data = concat(data, prompt_system);
                data = concat(data, conversation);
                data = concat(data, prompt_4);
                char *result = chatgpt_curl_perform(data, apikey, endpoint);
                free(data);
                if (result == NULL)
                {
                    conversation = prev_conversation;
                    continue;
                }
                printf("%s\n", result);
                if (show_usage)
                    printf("\n-- Used %d tokens (in total) --\n", tokens);
                conversation = concat(conversation, ",{\"role\": \"assistant\", \"content\": \"");
                conversation = concat(conversation, escape_string(result));
                conversation = concat(conversation, "\"}");
                free(result);
            }
            else if (apikey == NULL)
            {
                fprintf(stderr, "No API key provided. Please specify it with the /apikey shell command, or re-run the program with the '--setup' flag to configure it permanently.\n");
            }
            else
            {
                fprintf(stderr, "No message or command provided.\n");
            }
        }

        free(read_result);
    }
    return 0;
}

int setup(char *configdir, char *apikey, char *model)
{
    printf("Simple ChatGPT command-line utility for Unix-based systems.\n");
    printf("Application version: %s\n\n-", APP_VERSION);
    while (true)
    {
        printf("-- Current configuration ---\n");
        printf("API key: %s\n", apikey ? apikey : "(none)");
        printf("Model: %s\n-----------------------------\n", model ? model : "(none)");
        printf("What action do you want to perform?\n [1] %s API key\n [2] %s model\n [w] Save and exit\n [q] Exit without saving.\n",
               apikey == NULL ? "Set" : "Modify or unset",
               model == NULL ? "Set" : "Modify or unset");
        while (true)
        {
            printf("> ");
            char *selection = "", c = '\n';
            while ((c = getchar()) != EOF && c != '\n')
            {
                char str[2];
                str[0] = c;
                str[1] = '\0';
                selection = concat(selection, str);
            }
            if (strcmp(selection, "1") == 0)
            {
                printf("Please enter your OpenAI API key in the prompt below.\n");
                printf("You can obtain yours here: https://platform.openai.com/account/api-keys\n");
                printf("You might be billed for the usage you make within this application.\n");
                printf("To unset this value, please enter '(none)'.\n");
                printf("To return to the previous menu, press enter.\n[1]> ");
                char *apistr = "";
                c = '\n';
                while ((c = getchar()) != EOF && c != '\n')
                {
                    char str[2];
                    str[0] = c;
                    str[1] = '\0';
                    apistr = concat(apistr, str);
                }
                if (strcmp(apistr, "(none)") == 0)
                    apikey = NULL;
                else if (strlen(apistr) != 0)
                    apikey = apistr;
                break;
            }
            else if (strcmp(selection, "2") == 0)
            {
                printf("Please enter your OpenAI chat model in the prompt below.\n");
                printf("Check your available models: https://platform.openai.com/playground?mode=chat\n");
                printf("To unset this value, please enter '(none)'.\n");
                printf("To return to the previous menu, press enter.\n[2]> ");
                char *modelstr = "";
                c = '\n';
                while ((c = getchar()) != EOF && c != '\n')
                {
                    char str[2];
                    str[0] = c;
                    str[1] = '\0';
                    modelstr = concat(modelstr, str);
                }
                if (strcmp(modelstr, "(none)") == 0)
                    model = NULL;
                else if (strlen(modelstr) != 0)
                    model = modelstr;
                break;
            }
            else if (strcmp(selection, "w") == 0)
            {
                printf("Writing updated configuration...");
                FILE *fp = fopen(configdir, "w");
                if (fp == NULL)
                {
                    fprintf(stderr, " Error: cannot save data. Please try again later or check permissions.\n");
                    break;
                }
                if (apikey != NULL)
                    fprintf(fp, "apikey=%s\n", apikey);
                if (model != NULL)
                    fprintf(fp, "model=%s\n", model);
                if (apikey == NULL && model == NULL)
                    remove(configdir);
                fclose(fp);
                printf(" done!\nBye!\n");
                return 0;
            }
            else if (strcmp(selection, "q") == 0)
            {
                printf("Bye!\n");
                return 0;
            }
            else
            {
                fprintf(stderr, "Error: command not recognized.\n");
                continue;
            }
            break;
        }
    }
    return 0;
}

int help(char *prog_name)
{
    printf("Simple ChatGPT command-line utility for Unix-based systems.\n");
    printf("Application version: %s\n\n", APP_VERSION);
    printf("Usage: %s [ <prompt> | --setup | --help ]\n\n", prog_name);
    printf("Options:\n");
    printf(" <prompt>: The prompt (question) to send to ChatGPT.\n");
    printf("  --setup: Run client configuration wizard. Must be run once before using the client.\n");
    printf("   --help: Show this help message.\n\n");
    printf("If no arguments are specified, the program will enter in conversation (shell) mode.\n\n");
    printf("Example:\n");
    printf("    Input: %s Explain Linux in less than 20 words.\n", prog_name);
    printf("   Output: A free open-source operating system based on Unix that can run on most computer hardware.\n\n");
    printf("API usage might be billed. See https://openai.com/pricing for more information.\n");
    printf("This tool can save your conversation in shell mode until you exit or reset it.\n\n");
    printf("(C) 2023, Lumito - www.lumito.net. ChatGPT and API provided by OpenAI. This is not an official OpenAI application.\n");
    return 0;
}

int main(int argc, char **argv)
{
    char *homedir, *configdir, *apikey = NULL, *config = "";

    if ((homedir = getenv("HOME")) == NULL)
    {
        homedir = getpwuid(getuid())->pw_dir;
    }

    configdir = strcat(homedir, "/.chatgpt-client");

    FILE *fp = fopen(configdir, "r");
    if (fp == NULL)
    {
        if (argc < 2)
        {
            fprintf(stderr, "Error: No model configured. Please re-run the program with the '--setup' flag to configure it.\n");
            return 1;
        }
        if (argc > 1)
            if (strcmp(argv[1], "--help") == 0)
                return help(argv[0]);
        if (strcmp(argv[1], "--setup") == 0)
            return setup(configdir, NULL, NULL);
        fprintf(stderr, "Error: No API key found. Please re-run the program with the '--setup' flag to configure it.\n");
        return 1;
    }

    int f;
    while ((f = fgetc(fp)) != EOF)
    {
        if (f != '\r')
        {
            char str[2];
            str[0] = f;
            str[1] = '\0';
            config = concat(config, str);
        }
    }

    char *ptr1, *ptr2, *model = NULL;
    char *token = strtok_r(config, "\n", &ptr1);
    while (token != NULL)
    {
        if (strchr(token, '=') != NULL)
        {
            char *token2 = strtok_r(token, "=", &ptr2);
            unsigned short mode = 0;
            while (token2 != NULL)
            {
                if (mode == 1)
                {
                    apikey = strdup(token2);
                    mode = 0;
                }
                else if (mode == 2)
                {
                    model = strdup(token2);
                    mode = 0;
                }
                else if (!mode)
                {
                    if (strcmp(token2, "apikey") == 0)
                        mode = 1;
                    else if (strcmp(token2, "model") == 0)
                        mode = 2;
                }
                token2 = strtok_r(NULL, "=", &ptr2);
            }
        }
        token = strtok_r(NULL, "\n", &ptr1);
    }

    bool useapi = false;
    if (apikey != NULL)
        useapi = true;

    if (model == NULL)
    {
        if (argc > 1)
        {
            if (strcmp(argv[1], "--setup") == 0)
                return setup(configdir, useapi ? apikey : NULL, NULL);
            else if (strcmp(argv[1], "--help") == 0)
                return help(argv[0]);
        }
        else
        {
            fprintf(stderr, "Error: No model configured. Please re-run the program with the '--setup' flag to configure it.\n");
            return 1;
        }
    }
    else if (argc < 2)
        return shell_mode(useapi ? apikey : NULL, model);
    else if (argc > 1)
        if (strcmp(argv[1], "--help") == 0)
            return help(argv[0]);
    if (strcmp(argv[1], "--setup") == 0)
        return setup(configdir, useapi ? apikey : NULL, model);
    if (!useapi)
    {
        fprintf(stderr, "Error: No API key found. Please re-run the program with the '--setup' flag to configure it.\n");
        return 1;
    }

    char *prompt = "";

    size_t i = 0;
    for (; i < argc; i++)
    {
        prompt = concat(prompt, argv[i]);
        if (argc > i + 1)
            prompt = concat(prompt, " ");
    }

    char *data = "{\"model\": \"";

    data = concat(data, model);
    data = concat(data, "\", \"messages\": [{\"role\": \"user\", \"content\": \"");
    data = concat(data, prompt);
    data = concat(data, "\"}]}");

    char *res = chatgpt_curl_perform(data, apikey, "https://api.openai.com/v1/chat/completions");

    free(data);
    free(apikey);
    free(model);

    if (res == NULL)
        return 1;

    printf("%s\n", res);

    free(res);
    return 0;
}
