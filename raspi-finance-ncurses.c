#include <ncurses.h>
#include <form.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <curl/curl.h>
#include <uuid/uuid.h>

#define TRANSACTION_DATE "transactionDate"
#define ACCOUNT_NAME_OWNER "accountNameOwner"
#define AMOUNT "amount"
#define DESCRIPTION "description"
#define CATEGORY "category"
#define CLEARED "cleared"
#define NOTES "notes"
#define GUID "guid"
#define ACCOUNT_TYPE "accountType"
#define DATE_UPDATED "dateUpdated"
#define DATE_ADDED "dateAdded"

#define ACCOUNT_NAME_OWNER_IDX 0
#define ACCOUNT_TYPE_IDX 1
#define TRANSACTION_DATE_IDX 2
#define DESCRIPTION_IDX 3
#define CATEGORY_IDX 4
#define AMOUNT_IDX 5
#define CLEARED_IDX 6
#define NOTES_IDX 7

#define MAIN_MENU_LIST_SIZE 3

struct string {
  char *ptr;
  size_t len;
};

static FORM *form = NULL;
static FIELD *fields[17];
static WINDOW *win_body = NULL;
static WINDOW *win_form = NULL;
static WINDOW *win_main_menu = NULL;

int account_list_index = 0;

void show_main_screen();
void show_transaction_insert_screen();

char * trim_whitespaces( char *str ) {
    char *end = NULL;

    while(isspace(*str)) {
        str++;
    }

    if(*str == 0) {
        return str;
    }

    end = str + strnlen(str, 128) - 1;
    while(end > str && isspace(*end)) {
      end--;
    }

    *(end+1) = '\0';

    return str;
}

void setDefaultValues() {
    char today_string[30] = {0};
    time_t now = time(NULL);

    strftime(today_string, sizeof(today_string)-1, "%Y-%m-%dT12:00:00.000", localtime(&now));

    set_field_buffer(fields[TRANSACTION_DATE_IDX * 2], 0, TRANSACTION_DATE);
    set_field_buffer(fields[TRANSACTION_DATE_IDX * 2 + 1], 0, today_string);
    set_field_buffer(fields[DESCRIPTION_IDX * 2], 0, DESCRIPTION);
    set_field_buffer(fields[DESCRIPTION_IDX * 2 + 1], 0, "");
    set_field_buffer(fields[CATEGORY_IDX * 2], 0, CATEGORY);
    set_field_buffer(fields[CATEGORY_IDX * 2 + 1], 0, "");
    set_field_buffer(fields[AMOUNT_IDX * 2], 0, AMOUNT);
    set_field_buffer(fields[AMOUNT_IDX * 2 + 1], 0, "0.00");
    set_field_buffer(fields[CLEARED_IDX * 2], 0, CLEARED);
    set_field_buffer(fields[CLEARED_IDX * 2 + 1], 0, "0");
    set_field_buffer(fields[NOTES_IDX * 2], 0, NOTES);
    set_field_buffer(fields[NOTES_IDX * 2 + 1], 0, "");
    set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2], 0, ACCOUNT_NAME_OWNER);
    set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], 0, "");
    set_field_buffer(fields[ACCOUNT_TYPE_IDX * 2], 0, ACCOUNT_TYPE);
    set_field_buffer(fields[ACCOUNT_TYPE_IDX * 2 + 1], 0, "credit");
}

//int isActiveField( const FIELD *field ) {
//  if( field_opts(field) & O_ACTIVE ) {
//    return 1;
//  } else {
//    return 0;
//  }
//}

void init_string( struct string *s ) {
  s->len = 0;
  s->ptr = malloc(1);
  if (s->ptr == NULL) {
    fprintf(stderr, "malloc() failed\n");
    exit(EXIT_FAILURE);
  }
  s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
  size_t new_len = s->len + size*nmemb;
  s->ptr = realloc(s->ptr, new_len+1);
  if (s->ptr == NULL) {
    fprintf(stderr, "realloc() failed\n");
    exit(EXIT_FAILURE);
  }

  memcpy(s->ptr+s->len, ptr, size*nmemb);
  s->ptr[new_len] = '\0';
  s->len = new_len;

  return size*nmemb;
}

int curl_post_call(char *payload) {
    CURL *curl = curl_easy_init();
    CURLcode result;
    struct curl_slist *headers = NULL;
    struct string response;
    init_string(&response);

    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/transaction/insert");
    result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if( strcmp(response.ptr, "transaction inserted") == 0) {
      printw("200 - SUCCESS\n");
      free(response.ptr);
      return 0;
    } else {
      printw("curl - %s\n", response.ptr);
      free(response.ptr);
      return 1;
    }
}

char * extractField( const FIELD * field ) {
  return trim_whitespaces(field_buffer(field, 0));
}

int jsonPrintFieldValues() {
    char payload[500] = {0};
    uuid_t binuuid;
    char uuid[37] = {0};

    uuid_generate_random(binuuid);
    uuid_unparse_lower(binuuid, uuid);
    uuid_unparse(binuuid, uuid);

    for( int idx = 0; uuid[idx]; idx++ ) {
      uuid[idx] = tolower(uuid[idx]);
    }

    snprintf(payload + strlen(payload), sizeof(payload), "{");
    //snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%s,", TRANSACTION_DATE, extractField(fields[TRANSACTION_DATE_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", TRANSACTION_DATE, extractField(fields[TRANSACTION_DATE_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", DESCRIPTION, extractField(fields[DESCRIPTION_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", CATEGORY, extractField(fields[CATEGORY_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%s,", AMOUNT, extractField(fields[AMOUNT_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%s,", CLEARED, extractField(fields[CLEARED_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", NOTES, extractField(fields[NOTES_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", ACCOUNT_TYPE, extractField(fields[ACCOUNT_TYPE_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\",", ACCOUNT_NAME_OWNER, extractField(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1]));
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%lu,", DATE_ADDED, (unsigned long)time(NULL) * 1000);
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":%lu,", DATE_UPDATED, (unsigned long)time(NULL) * 1000);
    snprintf(payload + strlen(payload), sizeof(payload), "\"%s\":\"%s\"", GUID, uuid);
    snprintf(payload + strlen(payload), sizeof(payload), "}");
    int result = curl_post_call(payload);
    //wclear(win_body);
    //printw("%s", payload);
    return result;
}

static void driver(int ch) {
    char account_list[50][20] = {"chase_brian", "chase_brian", "usbank-cash_brian", "usbank-cash_kari", "amex_brian", "amex_kari", "barclays_kari", "barclays_brian", "citicash_brian" };

    switch (ch) {
        case KEY_F(4):
           set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], 0, "");
           set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], 0, account_list[--account_list_index % 9]);
        break;
        case KEY_F(5):
           set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], 0, "");
           set_field_buffer(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], 0, account_list[++account_list_index % 9]);
        break;
        case KEY_F(2):
            // Or the current field buffer won't be sync with what is displayed
            form_driver(form, REQ_NEXT_FIELD);
            form_driver(form, REQ_PREV_FIELD);
            move(LINES-3, 2);

            if( jsonPrintFieldValues() == 0 ) {
              setDefaultValues();
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

    wrefresh(win_form);
}

void cleanup_transaction_screen() {
    unpost_form(form);
    free_form(form);
    for( int idx = 0; idx < 16; idx++ ) {
      free_field(fields[idx]);
    }

    delwin(win_form);
    delwin(win_body);
    endwin();
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
    win_form = derwin(win_body, 20, 78, 3, 1);
    assert(win_form != NULL);
    box(win_form, 0, 0);
    curs_set(1);
    mvwprintw(win_body, 1, 2, "Press ESC to quit; F2 to save; F4/F5 back/forward account");

    fields[TRANSACTION_DATE_IDX * 2] = new_field(1, label_length, TRANSACTION_DATE_IDX * 2, 0, 0, 0);
    fields[TRANSACTION_DATE_IDX * 2 + 1] = new_field(1, text_length, TRANSACTION_DATE_IDX * 2, label_length + 1, 0, 0);
    fields[DESCRIPTION_IDX * 2] = new_field(1, label_length, DESCRIPTION_IDX * 2, 0, 0, 0);
    fields[DESCRIPTION_IDX * 2 + 1] = new_field(1, text_length, DESCRIPTION_IDX * 2, label_length + 1, 0, 0);
    fields[CATEGORY_IDX * 2] = new_field(1, label_length, CATEGORY_IDX * 2, 0, 0, 0);
    fields[CATEGORY_IDX * 2 + 1] = new_field(1, text_length, CATEGORY_IDX * 2, label_length + 1, 0, 0);
    fields[AMOUNT_IDX * 2] = new_field(1, label_length, AMOUNT_IDX * 2, 0, 0, 0);
    fields[AMOUNT_IDX * 2 + 1] = new_field(1, text_length, AMOUNT_IDX * 2, label_length + 1, 0, 0);
    fields[CLEARED_IDX * 2] = new_field(1, label_length, CLEARED_IDX * 2, 0, 0, 0);
    fields[CLEARED_IDX * 2 + 1] = new_field(1, text_length, CLEARED_IDX * 2, label_length + 1, 0, 0);
    fields[NOTES_IDX * 2] = new_field(1, label_length, NOTES_IDX * 2, 0, 0, 0);
    fields[NOTES_IDX * 2 + 1] = new_field(1, text_length, NOTES_IDX * 2, label_length + 1, 0, 0);
    fields[ACCOUNT_NAME_OWNER_IDX * 2] = new_field(1, label_length, ACCOUNT_NAME_OWNER_IDX * 2, 0, 0, 0);
    fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1] = new_field(1, text_length, ACCOUNT_NAME_OWNER_IDX * 2, label_length + 1, 0, 0);
    fields[ACCOUNT_TYPE_IDX * 2] = new_field(1, label_length, ACCOUNT_TYPE_IDX * 2, 0, 0, 0);
    fields[ACCOUNT_TYPE_IDX * 2 + 1] = new_field(1, text_length, ACCOUNT_TYPE_IDX * 2, label_length + 1, 0, 0);
    fields[16] = NULL;

    for( int idx = 0; idx < 16; idx++ ) {
      assert(fields[idx] != NULL);
    }

    setDefaultValues();

    set_field_opts(fields[TRANSACTION_DATE_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[TRANSACTION_DATE_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[DESCRIPTION_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[DESCRIPTION_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[CATEGORY_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[CATEGORY_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[AMOUNT_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[AMOUNT_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[CLEARED_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[CLEARED_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[NOTES_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[NOTES_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[ACCOUNT_NAME_OWNER_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
    set_field_opts(fields[ACCOUNT_TYPE_IDX * 2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
    set_field_opts(fields[ACCOUNT_TYPE_IDX * 2 + 1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);

    set_field_back(fields[TRANSACTION_DATE_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[DESCRIPTION_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[CATEGORY_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[AMOUNT_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[CLEARED_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[NOTES_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1], A_UNDERLINE);
    set_field_back(fields[ACCOUNT_TYPE_IDX * 2 + 1], A_UNDERLINE);

    form = new_form(fields);
    assert(form != NULL);
    set_form_win(form, win_form);
    set_form_sub(form, derwin(win_form, 18, 76, 1, 1));
    post_form(form);

    refresh();
    wrefresh(win_body);
    wrefresh(win_form);

    //while ((ch = wgetch(win_body)) != 27) { //escape = 27
    while ((ch = getch()) != 27) { //escape = 27
        driver(ch);
    }

  cleanup_transaction_screen();
  show_main_screen();
}

void show_main_screen() {
    //int list_size = 6;
    char list[MAIN_MENU_LIST_SIZE][12] = { "transaction", "payment", "quit" };
    char item[12] = {0};
    int ch = 0;
    int idx = 0;

    initscr(); // initialize Ncurses
    //win_main_menu = newwin(10, 15, 1, 1); // create a new window
    win_main_menu = newwin(24, 80, 0, 0);; // create a new window
    assert(win_main_menu != NULL);
    box(win_main_menu, 0, 0); // sets default borders for the window

// now print all the menu items and highlight the first one
    for( idx = 0; idx < MAIN_MENU_LIST_SIZE; idx++ ) {
        if( idx == 0 ) {
            wattron(win_main_menu, A_STANDOUT); // highlights the first item.
        } else {
            wattroff(win_main_menu, A_STANDOUT);
        }
        snprintf(item, sizeof(item), "%s", list[idx]);
        mvwprintw(win_main_menu, idx + 1, 2, "%s", item);
    }

    wrefresh(win_main_menu); // update the terminal screen

    idx = 0;
    noecho(); // disable echoing of characters on the screen
    keypad(win_main_menu, TRUE); // enable keyboard input for the window.
    curs_set(0); // hide the default screen cursor.

    ch = wgetch(win_main_menu);
    while( ch != 27 ) {
        snprintf(item, sizeof(item), "%s",  list[idx]);
        mvwprintw(win_main_menu, idx + 1, 2, "%s", item );
        switch( ch ) {
            case KEY_UP:
            case 'k':
                idx--;
                idx = ( idx < 0 ) ? (MAIN_MENU_LIST_SIZE-1) : idx;
                break;
            case '\n':
                if( idx == 0 ) {
                    wclear(win_main_menu);
                    delwin(win_main_menu);
                    endwin();
                    show_transaction_insert_screen();
                }

                if( idx == 1 ) {
                    printw("payment.");
                }

                if( idx == 2 ) {
                  wclear(win_main_menu);
                  delwin(win_main_menu);
                  endwin();
                  exit(0);
                }
            break;
            case KEY_DOWN:
            case 'j':
                idx++;
                idx = ( idx > (MAIN_MENU_LIST_SIZE-1) ) ? 0 : idx;
                break;
        }
        // now highlight the next item in the list.
        wattron(win_main_menu, A_STANDOUT);

        //sprintf(item, "%-7s",  list[i]);
        snprintf(item, sizeof(item), "%s",  list[idx]);
        mvwprintw(win_main_menu, idx + 1, 2, "%s", item);
        wattroff(win_main_menu, A_STANDOUT);

        ch = wgetch(win_main_menu);
    }

    delwin(win_main_menu);
    endwin();
    exit(EXIT_SUCCESS);
}

int main(int arg, char *argv[]) {
  show_main_screen();
  return 0;
}
