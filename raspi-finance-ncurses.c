#include <ncurses.h>
#include <form.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>
#include <uuid/uuid.h>
#include <cjson/cJSON.h>
#include <errno.h>

#define TRANSACTION_DATE "transactionDate"
#define ACCOUNT_NAME_OWNER "accountNameOwner"
#define AMOUNT "amount"
#define DESCRIPTION "description"
#define CATEGORY "category"
#define TRANSACTION_STATE "transactionState"
#define NOTES "notes"
#define GUID "guid"
#define REOCCURRING "reoccurring"
#define ACCOUNT_TYPE "accountType"
#define DATE_UPDATED "dateUpdated"
#define DATE_ADDED "dateAdded"
#define SPECIFIC_DAY "specificDay"
#define MONTH_END "monthEnd"

#define TRANSACTION_INSERT_URL "https://hornsup:8080/transaction/insert"
#define PAYMENT_INSERT_URL "https://hornsup:8080/payment/insert"
#define REOCCURRING_CLONE_URL "https://hornsup:8080/transaction/clone"

#define ESCAPE_CHAR 27

#define SUCCESS 0
#define FAILURE 1

#define MAX_PAYLOAD 500
#define MAX_ACCOUNT_NAME_OWNER_LENGTH 100

typedef enum {
    TRANSACTION_ACCOUNT_NAME_OWNER = 0,
    TRANSACTION_ACCOUNT_TYPE,
    TRANSACTION_TRANSACTION_DATE,
    TRANSACTION_DESCRIPTION,
    TRANSACTION_CATEGORY,
    TRANSACTION_AMOUNT,
    TRANSACTION_TRANSACTION_STATE,
    TRANSACTION_NOTES,

    MAX_TRANSACTION
} TransactionIndex;

typedef enum {
    PAYMENT_ACCOUNT_NAME_OWNER = 0,
    PAYMENT_TRANSACTION_DATE,
    PAYMENT_AMOUNT,

    MAX_PAYMENT
} PaymentIndex;

typedef enum {
    REOCCURRING_GUID = 0,
    REOCCURRING_SPECIFIC_DAY,
    REOCCURRING_AMOUNT,
    REOCCURRING_MONTH_END,

    MAX_REOCCURRING
} ReoccurringIndex;

typedef enum {
    MENU_TYPE_TRANSACTION = 0,
    MENU_TYPE_PAYMENT,
    MENU_TYPE_REOCCURRING,
    MENU_TYPE_QUIT,

    MAX_MENU_TYPE
} MenuType;

typedef struct {
  char *ptr;
  size_t len;
} String;

//TODO: menu_list make that enum?
static const char *menu_list[] = {"transaction", "payment", "reoccurring", "quit"};

char **account_list = NULL;
long account_list_size = 0L;

typedef struct {
  char **account_list;
  size_t size;
} AccountList;

AccountList *account_list_new = NULL;

//TODO: eradicate these global vars
static FORM *form = NULL;
static FIELD *fields[MAX_TRANSACTION * 2 + 1];
static WINDOW *win_body = NULL;
static WINDOW *main_window = NULL;

int current_account_list_index = 0;

void show_main_screen();
void show_transaction_insert_screen();
void set_transaction_default_values();
void account_name_rotate_backward( int );
void account_name_rotate_forward( int );
void cleanup_payment_screen( MenuType );
void driver_screens( int, MenuType );
void init_string( String * );
void set_payment_default_values();
void show_payment_insert_screen();
char * extract_field( const FIELD * );
char * trim_whitespaces( char * );
int curl_post_call( char *, MenuType );
int payment_json_generated();
int transaction_json_generated();
void create_string_array( long );
void show_reoccurring_insert_screen();
int reoccurring_json_generated();

long parse_long(const char *str) {
    errno = 0;
    char *temp;
    long val = strtol(str, &temp, 0);

    if (temp == str || *temp != '\0' || ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
        fprintf(stderr, "Could not convert '%s' to long and leftover string is: '%s'\n", str, temp);
    }
    return val;
}

int jq_fetch_accounts_count() {
  FILE *fp = NULL;
  char count[10] = {0};
  long value = 0;

  fp = popen("curl -s --cacert hornsup-raspi-finance-cert.pem -X GET 'https://hornsup:8080/account/select/active' | jq '.[] | .accountNameOwner' | wc -l", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }

  fgets(count, sizeof(count), fp);
  pclose(fp);
  //TODO: trim_whitespaces();
  value = parse_long(count);
  return value;
}

void jq_fetch_accounts() {
  FILE *fp = NULL;
  char account_name_owner[MAX_ACCOUNT_NAME_OWNER_LENGTH] = {0};
  long index = 0;

  account_list_size = jq_fetch_accounts_count();
  create_string_array(account_list_size);
  fp = popen("curl -s --cacert hornsup-raspi-finance-cert.pem -X GET 'https://hornsup:8080/account/select/active' | jq '.[] | .accountNameOwner' | tr -d '\"'", "r");
  if (fp == NULL) {
    printf("Failed to run command\n");
    exit(1);
  }

  while (fgets(account_name_owner, sizeof(account_name_owner), fp) != NULL) {
    snprintf(account_list[index], MAX_ACCOUNT_NAME_OWNER_LENGTH, "%s", account_name_owner);
    index++;
  }
  pclose(fp);
}

void create_string_array( long list_size ) {
  //TODO: create an instance of tye structure
  //account_list_new = calloc()

  account_list = calloc(list_size, sizeof(char*));
  for ( int idx = 0; idx < list_size; idx++ ) {
    account_list[idx] = calloc((MAX_ACCOUNT_NAME_OWNER_LENGTH + 1), sizeof(char));
  }
}

char * trim_whitespaces( char *str ) {
    char *end = NULL;
    char *trimmed = str;

    while( isspace(*trimmed) ) {
        trimmed++;
    }

    if( *trimmed == '\0' ) {
        return trimmed;
    }

    end = trimmed + strnlen(trimmed, 128) - 1;
    while(end > trimmed && isspace(*end)) {
      end--;
    }

    *(end + 1) = '\0';

    return trimmed;
}

void init_string( String *s ) {
  s->len = 0;
  //TODO: use calloc
  s->ptr = malloc(1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t write_response_to_string( void *ptr, size_t size, size_t nmemb, String *s ) {
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len + 1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }

  //TODO: should I use snprintf
  memcpy(s->ptr+s->len, ptr, size * nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size * nmemb;
}

int curl_post_call( char *payload, MenuType menu_type ) {
    CURL *curl = curl_easy_init();
    //CURLcode result;
    struct curl_slist *headers = NULL;
    String response = {0};
    char *certFileName = "hornsup-raspi-finance-cert.pem";
    /* char *keyFileName = "hornsup-raspi-finance-key.pem"; */
    init_string(&response);

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 1L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, certFileName);
    /* curl_easy_setopt(curl, CURLOPT_SSLCERT, certFileName); */
    /* curl_easy_setopt(curl, CURLOPT_SSLKEY, keyFileName); */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    if( menu_type == MENU_TYPE_TRANSACTION ) {
      curl_easy_setopt(curl, CURLOPT_URL, TRANSACTION_INSERT_URL);
      printw("transaction:");
    } else if( menu_type == MENU_TYPE_PAYMENT ) {
      curl_easy_setopt(curl, CURLOPT_URL, PAYMENT_INSERT_URL);
      printw("payment:");
    } else if( menu_type == MENU_TYPE_REOCCURRING ) {
      curl_easy_setopt(curl, CURLOPT_URL, REOCCURRING_CLONE_URL);
      printw("reoccurring:");
    } else {
      printw("invalid type.");
      free(response.ptr);
      response.ptr = NULL;
      return FAILURE;
    }
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    //TODO: add a payment check like the one below
    if( strcmp(response.ptr, "transaction inserted") == 0) {
      printw("200 - SUCCESS\n");
      free(response.ptr);
      response.ptr = NULL;
      return SUCCESS;
    } else if(strcmp(response.ptr, "payment inserted") == 0) {
      printw("200 - SUCCESS\n");
      free(response.ptr);
      response.ptr = NULL;
      return SUCCESS;
    } else {
      if( response.len > 0) {
        printw("failure: %s\n", response.ptr);
      } else {
        printw("connect failed: ");
      }
      free(response.ptr);
      response.ptr = NULL;
      return FAILURE;
    }
}

char * extract_field( const FIELD * field ) {
  return trim_whitespaces(field_buffer(field, 0));
}

void set_transaction_default_values() {
    char today_string[30] = {0};
    time_t now = time(NULL);

    strftime(today_string, sizeof(today_string)-1, "%Y-%m-%dT12:00:00.000", localtime(&now));

    set_field_buffer(fields[TRANSACTION_TRANSACTION_DATE * 2], 0, TRANSACTION_DATE);
    set_field_buffer(fields[TRANSACTION_TRANSACTION_DATE * 2 + 1], 0, today_string);
    set_field_buffer(fields[TRANSACTION_DESCRIPTION * 2], 0, DESCRIPTION);
    set_field_buffer(fields[TRANSACTION_DESCRIPTION * 2 + 1], 0, "");
    set_field_buffer(fields[TRANSACTION_CATEGORY * 2], 0, CATEGORY);
    set_field_buffer(fields[TRANSACTION_CATEGORY * 2 + 1], 0, "");
    set_field_buffer(fields[TRANSACTION_AMOUNT * 2], 0, AMOUNT);
    set_field_buffer(fields[TRANSACTION_AMOUNT * 2 + 1], 0, "");
    set_field_buffer(fields[TRANSACTION_TRANSACTION_STATE * 2], 0, TRANSACTION_STATE);
    set_field_buffer(fields[TRANSACTION_TRANSACTION_STATE * 2 + 1], 0, "outstanding");
    set_field_buffer(fields[TRANSACTION_NOTES * 2], 0, NOTES);
    set_field_buffer(fields[TRANSACTION_NOTES * 2 + 1], 0, "processed by ncurses");
    set_field_buffer(fields[TRANSACTION_ACCOUNT_NAME_OWNER * 2], 0, ACCOUNT_NAME_OWNER);
    set_field_buffer(fields[TRANSACTION_ACCOUNT_NAME_OWNER * 2 + 1], 0, "");
    set_field_buffer(fields[TRANSACTION_ACCOUNT_TYPE * 2], 0, ACCOUNT_TYPE);
    set_field_buffer(fields[TRANSACTION_ACCOUNT_TYPE * 2 + 1], 0, "credit");
}

void set_payment_default_values() {
    char today_string[30] = {0};
    time_t now = time(NULL);

    strftime(today_string, sizeof(today_string)-1, "%Y-%m-%dT12:00:00.000", localtime(&now));

    set_field_buffer(fields[PAYMENT_TRANSACTION_DATE * 2], 0, TRANSACTION_DATE);
    set_field_buffer(fields[PAYMENT_TRANSACTION_DATE * 2 + 1], 0, today_string);
    set_field_buffer(fields[PAYMENT_AMOUNT * 2], 0, AMOUNT);
    set_field_buffer(fields[PAYMENT_AMOUNT * 2 + 1], 0, "");
    set_field_buffer(fields[PAYMENT_ACCOUNT_NAME_OWNER * 2], 0, ACCOUNT_NAME_OWNER);
    set_field_buffer(fields[PAYMENT_ACCOUNT_NAME_OWNER * 2 + 1], 0, "");
}

void set_reoccurring_default_values() {
    set_field_buffer(fields[REOCCURRING_SPECIFIC_DAY * 2], 0, SPECIFIC_DAY);
    set_field_buffer(fields[REOCCURRING_SPECIFIC_DAY * 2 + 1], 0, "1");
    set_field_buffer(fields[REOCCURRING_AMOUNT * 2], 0, AMOUNT);
    set_field_buffer(fields[REOCCURRING_AMOUNT * 2 + 1], 0, "0.00");
    set_field_buffer(fields[REOCCURRING_GUID * 2], 0, GUID);
    set_field_buffer(fields[REOCCURRING_GUID * 2 + 1], 0, "");
    set_field_buffer(fields[REOCCURRING_MONTH_END * 2], 0, MONTH_END);
    set_field_buffer(fields[REOCCURRING_MONTH_END * 2 + 1], 0, "false");
}

int transaction_json_generated() {
    char payload[MAX_PAYLOAD] = {0};
    uuid_t binuuid;
    char uuid[37] = {0};

    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, uuid);
    uuid_unparse(binuuid, uuid);

    for( int idx = 0; uuid[idx] != '\0'; idx++ ) {
      uuid[idx] = tolower(uuid[idx]);
    }

    snprintf(payload + strlen(payload), sizeof(payload), "{");
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", TRANSACTION_DATE, extract_field(fields[TRANSACTION_TRANSACTION_DATE * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", DESCRIPTION, extract_field(fields[TRANSACTION_DESCRIPTION * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", CATEGORY, extract_field(fields[TRANSACTION_CATEGORY * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%s,", AMOUNT, extract_field(fields[TRANSACTION_AMOUNT * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", TRANSACTION_STATE, extract_field(fields[TRANSACTION_TRANSACTION_STATE * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", NOTES, extract_field(fields[TRANSACTION_NOTES * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", ACCOUNT_TYPE, extract_field(fields[TRANSACTION_ACCOUNT_TYPE * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", ACCOUNT_NAME_OWNER, extract_field(fields[TRANSACTION_ACCOUNT_NAME_OWNER * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":false,", REOCCURRING);
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\"", GUID, uuid);
    snprintf(payload + strlen(payload), sizeof(payload), "}");

    //TODO: add json logic
    //cJSON *json = cJSON_ParseWithLength(payload, sizeof(payload));
    //cJSON *json = cJSON_ParseWithOpts(payload, sizeof(payload));

    int result = curl_post_call(payload, MENU_TYPE_TRANSACTION);
    //wclear(win_body);
    //printw("%s", payload);
    return result;
}

int reoccurring_json_generated() {
    char payload[MAX_PAYLOAD] = {0};
    strncat(payload, "{\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, GUID, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[REOCCURRING_GUID * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\",", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, SPECIFIC_DAY, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[REOCCURRING_SPECIFIC_DAY * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\",", MAX_PAYLOAD - strlen(payload) - 1);

    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, AMOUNT, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[REOCCURRING_AMOUNT * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, ",", MAX_PAYLOAD - strlen(payload) - 1);

    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, MONTH_END, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[REOCCURRING_MONTH_END * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    //strncat(payload, "\"}", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "}", MAX_PAYLOAD - strlen(payload) - 1);
    int result = curl_post_call(payload, MENU_TYPE_REOCCURRING);
    printw("%s", payload);
    return result;
}

int payment_json_generated() {
    char payload[MAX_PAYLOAD] = {0};
    strncat(payload, "{\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, TRANSACTION_DATE, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[PAYMENT_TRANSACTION_DATE * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\",", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, AMOUNT, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[PAYMENT_AMOUNT * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, ",", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, ACCOUNT_NAME_OWNER, MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\":", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"", MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, extract_field(fields[PAYMENT_ACCOUNT_NAME_OWNER * 2 + 1]), MAX_PAYLOAD - strlen(payload) - 1);
    strncat(payload, "\"}", MAX_PAYLOAD - strlen(payload) - 1);
    int result = curl_post_call(payload, MENU_TYPE_PAYMENT);
    printw("%s", payload);
    return result;
}

void account_name_rotate_backward( int idx ) {
   //int account_list_size = sizeof(account_list)/sizeof(char *);

   if( account_list_size > 0 ) {
       set_field_buffer(fields[idx * 2 + 1], 0, "");
       /* idx--; */
       /* idx = ( idx < 0 ) ? (menu_list_size-1) : idx; */
       current_account_list_index = (current_account_list_index > 0 ) ? current_account_list_index - 1 : account_list_size - 1;
       set_field_buffer(fields[idx * 2 + 1], 0, account_list[current_account_list_index % account_list_size]);
   }
}

void account_name_rotate_forward( int idx ) {
    //int account_list_size = sizeof(account_list)/sizeof(char *);

    if( account_list_size > 0 ) {
        /* idx++; */
        /* idx = ( idx > (menu_list_size-1) ) ? 0 : idx; */
        set_field_buffer(fields[idx * 2 + 1], 0, "");
        set_field_buffer(fields[idx * 2 + 1], 0, account_list[++current_account_list_index % account_list_size]);
    }
}

void driver_screens( int ch, MenuType menu_type ) {
    switch (ch) {
        case KEY_F(4):
            if( menu_type == MENU_TYPE_TRANSACTION ) {
              account_name_rotate_backward(TRANSACTION_ACCOUNT_NAME_OWNER);
            } else if( menu_type == MENU_TYPE_PAYMENT ) {
              account_name_rotate_backward(PAYMENT_ACCOUNT_NAME_OWNER);
            } else if( menu_type == MENU_TYPE_REOCCURRING ) {
            }

        break;
        case KEY_F(5):
            if( menu_type == MENU_TYPE_TRANSACTION ) {
              account_name_rotate_forward(TRANSACTION_ACCOUNT_NAME_OWNER);
            } else if( menu_type == MENU_TYPE_PAYMENT ) {
              account_name_rotate_forward(PAYMENT_ACCOUNT_NAME_OWNER);
            } else if( menu_type == MENU_TYPE_REOCCURRING ) {
            }
        break;
        case KEY_F(2):
            // Or the current field buffer won't be sync with what is displayed
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_PREV_FIELD);
            move(LINES-3, 2);

            if( menu_type == MENU_TYPE_TRANSACTION ) {
                if( transaction_json_generated() == SUCCESS ) {
                  set_transaction_default_values();
                }
            } else if( menu_type == MENU_TYPE_PAYMENT ) {
                if( payment_json_generated() == SUCCESS ) {
                  set_payment_default_values();
                }
            } else if( menu_type == MENU_TYPE_REOCCURRING ) {
                if( reoccurring_json_generated() == SUCCESS ) {
                  set_reoccurring_default_values();
                }
            }

            refresh();
            pos_form_cursor(form);
            break;
        case 353: //shift tab
            form_driver(form, REQ_PREV_FIELD);
            break;
        case KEY_DOWN:
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_END_LINE);
            break;
        case '\t':
        case '\n':
            form_driver(form, REQ_NEXT_FIELD);
            break;
        case KEY_UP:
            form_driver(form, REQ_PREV_FIELD);
            form_driver(form, REQ_END_LINE);
            break;
        case KEY_LEFT:
            form_driver(form, REQ_PREV_CHAR);
            break;
        case KEY_RIGHT:
            form_driver(form, REQ_NEXT_CHAR);
            break;
        // Delete the char before cursor
        case KEY_BACKSPACE:
        case 127:
            form_driver(form, REQ_DEL_PREV);
            break;
        // Delete the char under the cursor
        case KEY_DC:
            form_driver(form, REQ_DEL_CHAR);
            break;
        default:
            form_driver(form, ch);
            break;
    }

    wrefresh(main_window);
}

void cleanup_screen( MenuType menu_type ) {
    if( form != NULL ) {
      unpost_form(form);
      free_form(form);
      form = NULL;
    }

    if( menu_type == MENU_TYPE_TRANSACTION ) {
      for( int idx = 0; idx < MAX_TRANSACTION * 2; idx++ ) {
        free_field(fields[idx]);
        fields[idx] = NULL;
      }
    } else if( menu_type == MENU_TYPE_PAYMENT ) {
      for( int idx = 0; idx < MAX_PAYMENT * 2; idx++ ) {
        free_field(fields[idx]);
        fields[idx] = NULL;
      }
    } else if( menu_type == MENU_TYPE_REOCCURRING ) {
      for( int idx = 0; idx < MAX_REOCCURRING * 2; idx++ ) {
        free_field(fields[idx]);
        fields[idx] = NULL;
      }
    } else {
    }

    delwin(main_window);
    delwin(win_body);
    endwin();
}

void show_payment_insert_screen() {
    int ch = 0;
    int label_length = 16;
    int text_length = 40;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    win_body = newwin(24, 80, 0, 0);
    assert(win_body != NULL);
    box(win_body, 0, 0);
    main_window = derwin(win_body, 20, 78, 3, 1);
    assert(main_window != NULL);
    box(main_window, 0, 0);
    curs_set(1);
    mvwprintw(win_body, 1, 2, "Press ESC to quit; F2 to save; F4/F5 back/forward account");

    for( int idx = 0; idx < MAX_PAYMENT; idx++ ) {
        fields[idx * 2] = new_field(1, label_length, idx * 2, 0, 0, 0);
        fields[idx * 2 + 1] = new_field(1, text_length, idx * 2, label_length + 1, 0, 0);
    }

    fields[MAX_PAYMENT * 2] = NULL;

    for( int idx = 0; idx < MAX_PAYMENT * 2; idx++ ) {
      assert(fields[idx] != NULL);
    }

    set_payment_default_values();

    for( int idx = 0; idx < MAX_PAYMENT; idx++ ) {
      set_field_opts(fields[idx * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
      set_field_opts(fields[idx * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
      set_field_back(fields[idx * 2 + 1], A_UNDERLINE);
    }

    form = new_form(fields);
    assert(form != NULL);
    set_form_win(form, main_window);
    set_form_sub(form, derwin(main_window, 18, 76, 1, 1));
    post_form(form);

    refresh();
    wrefresh(win_body);
    wrefresh(main_window);

    while ((ch = getch()) != ESCAPE_CHAR) {
        driver_screens(ch, MENU_TYPE_PAYMENT);
    }

  cleanup_screen(MENU_TYPE_PAYMENT);
  show_main_screen();
}

void show_reoccurring_insert_screen() {
    int ch = 0;
    int label_length = 16;
    int text_length = 40;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    win_body = newwin(24, 80, 0, 0);
    assert(win_body != NULL);
    box(win_body, 0, 0);
    main_window = derwin(win_body, 20, 78, 3, 1);
    assert(main_window != NULL);
    box(main_window, 0, 0);
    curs_set(1);
    mvwprintw(win_body, 1, 2, "Press ESC to quit; F2 to save; F4/F5 back/forward account");

    for( int idx = 0; idx < MAX_REOCCURRING; idx++ ) {
        fields[idx * 2] = new_field(1, label_length, idx * 2, 0, 0, 0);
        fields[idx * 2 + 1] = new_field(1, text_length, idx * 2, label_length + 1, 0, 0);
    }

    fields[MAX_REOCCURRING * 2] = NULL;

    for( int idx = 0; idx < MAX_REOCCURRING * 2; idx++ ) {
      assert(fields[idx] != NULL);
    }

    set_reoccurring_default_values();

    for( int idx = 0; idx < MAX_REOCCURRING; idx++ ) {
      set_field_opts(fields[idx * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
      set_field_opts(fields[idx * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
      set_field_back(fields[idx * 2 + 1], A_UNDERLINE);
    }

    form = new_form(fields);
    assert(form != NULL);
    set_form_win(form, main_window);
    set_form_sub(form, derwin(main_window, 18, 76, 1, 1));
    post_form(form);

    refresh();
    wrefresh(win_body);
    wrefresh(main_window);

    while ((ch = getch()) != ESCAPE_CHAR) {
        driver_screens(ch, MENU_TYPE_REOCCURRING);
    }

  cleanup_screen(MENU_TYPE_REOCCURRING);
  show_main_screen();
}

void show_transaction_insert_screen() {
    int ch = 0;
    int label_length = 16;
    int text_length = 40;
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    win_body = newwin(24, 80, 0, 0);
    assert(win_body != NULL);
    box(win_body, 0, 0);
    main_window = derwin(win_body, 20, 78, 3, 1);
    assert(main_window != NULL);
    box(main_window, 0, 0);
    curs_set(1);
    mvwprintw(win_body, 1, 2, "Press ESC to quit; F2 to save; F4/F5 back/forward account");

    for( int idx = 0; idx < MAX_TRANSACTION; idx++ ) {
        fields[idx * 2] = new_field(1, label_length, idx * 2, 0, 0, 0);
        fields[idx * 2 + 1] = new_field(1, text_length, idx * 2, label_length + 1, 0, 0);
    }

    fields[MAX_TRANSACTION * 2] = NULL;

    for( int idx = 0; idx < MAX_TRANSACTION * 2; idx++ ) {
      assert(fields[idx] != NULL);
    }

    set_transaction_default_values();

    for( int idx = 0; idx < MAX_TRANSACTION; idx++ ) {
      set_field_opts(fields[idx * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
      set_field_opts(fields[idx * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
      set_field_back(fields[idx * 2 + 1], A_UNDERLINE);
    }

    form = new_form(fields);
    assert(form != NULL);
    set_form_win(form, main_window);
    set_form_sub(form, derwin(main_window, 18, 76, 1, 1));
    post_form(form);

    refresh();
    wrefresh(win_body);
    wrefresh(main_window);

    while ((ch = getch()) != ESCAPE_CHAR) {
        driver_screens(ch, MENU_TYPE_TRANSACTION);
    }

  cleanup_screen(MENU_TYPE_TRANSACTION);
  show_main_screen();
}

void show_main_screen() {
    char item[12] = {0};
    int ch = 0;
    int idx = 0;
    int menu_list_size = sizeof(menu_list)/sizeof(char *);

    initscr(); // initialize Ncurses
    //main_window = newwin(10, 15, 1, 1); // create a new window
    main_window = newwin(24, 80, 0, 0);; // create a new window
    assert(main_window != NULL);
    box(main_window, 0, 0); // sets default borders for the window

    for( idx = 0; idx < menu_list_size; idx++ ) {
        if( idx == 0 ) {
            wattron(main_window, A_STANDOUT); // highlights the first item.
        } else {
            wattroff(main_window, A_STANDOUT);
        }
        snprintf(item, sizeof(item), "%s", menu_list[idx]);
        mvwprintw(main_window, idx + 1, 2, "%s", item);
    }

    wrefresh(main_window); // update the terminal screen

    idx = 0;
    noecho(); // disable echoing of characters on the screen
    keypad(main_window, TRUE); // enable keyboard input for the window.
    curs_set(0); // hide the default screen cursor.

    ch = wgetch(main_window);
    while( ch != ESCAPE_CHAR ) {
        snprintf(item, sizeof(item), "%s",  menu_list[idx]);
        mvwprintw(main_window, idx + 1, 2, "%s", item );
        switch( ch ) {
            case KEY_UP:
            case 'k':
                idx--;
                idx = ( idx < 0 ) ? (menu_list_size-1) : idx;
                break;
            case '\n':
                if( idx == MENU_TYPE_TRANSACTION ) {
                    wclear(main_window);
                    delwin(main_window);
                    endwin();
                    show_transaction_insert_screen();
                }

                if( idx == MENU_TYPE_PAYMENT ) {
                    wclear(main_window);
                    delwin(main_window);
                    endwin();
                    show_payment_insert_screen();
                }

                if( idx == MENU_TYPE_REOCCURRING ) {
                    wclear(main_window);
                    delwin(main_window);
                    endwin();
                    show_reoccurring_insert_screen();
                }


                if( idx == MENU_TYPE_QUIT ) {
                  wclear(main_window);
                  delwin(main_window);
                  endwin();
                  exit(0);
                }
            break;
            case KEY_DOWN:
            case 'j':
                idx++;
                idx = ( idx > (menu_list_size-1) ) ? 0 : idx;
                break;
        }
        // now highlight the next item in the list.
        wattron(main_window, A_STANDOUT);

        //sprintf(item, "%-7s",  list[i]);
        snprintf(item, sizeof(item), "%s",  menu_list[idx]);
        mvwprintw(main_window, idx + 1, 2, "%s", item);
        wattroff(main_window, A_STANDOUT);

        ch = wgetch(main_window);
    }

    delwin(main_window);
    endwin();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  if( argc != 1 ) {
    fprintf( stderr, "Usage: %s <noargs>\n", argv[0] );
    exit(1);
  }

  jq_fetch_accounts();
  show_main_screen();
  return 0;
}
