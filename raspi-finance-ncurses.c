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
#define DESCRIPTION "description"
#define CATEGORY "category"
#define AMOUNT "amount"
#define CLEARED "cleared"
#define NOTES "notes"
#define ACCOUNT_NAME_OWNER "accountNameOwner"
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

#define MAIN_MENU_LIST_SIZE 6

static FORM *form = NULL;
static FIELD *fields[17];
static WINDOW *win_body = NULL;
static WINDOW *win_form = NULL;
static WINDOW *win_main_menu = NULL;

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
    char today_string[20] = {0};
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    //strftime(today_string, sizeof(today_string)-1, "%m/%d/%Y", t);
    strftime(today_string, sizeof(today_string)-1, "%Y-%m-%d", t);

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

int isActiveField( const FIELD *field ) {
  if( field_opts(field) & O_ACTIVE ) {
    return 1;
  } else {
    return 0;
  }
}

size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp) {
   return size * nmemb;
}

int curl_post_call(char *payload) {
    CURL *curl = curl_easy_init();
    CURLcode result;
    struct curl_slist *headers = NULL;
    //FILE *devnull = fopen("/dev/null", "w+");
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charset: utf-8");

    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/transaction/insert");
    result = curl_easy_perform(curl);
    if( result == CURLE_OK ) {
      return 0;
    }
    return 1;
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

    for(int idx = 0; uuid[idx]; idx++){
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
    printw("%s", payload);
    printw("-- %d", result);
    return result;
}

static void driver(int ch) {
	switch (ch) {
		case KEY_F(2):
			// Or the current field buffer won't be sync with what is displayed
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_PREV_FIELD);
			move(LINES-3, 2);

            int result = jsonPrintFieldValues();
            if( result == 0 ) {
			  setDefaultValues();
			}

			refresh();
			pos_form_cursor(form);
			break;

        case 353: //shift tab
            form_driver(form, REQ_PREV_FIELD);
            break;
        case KEY_F(12):
            setDefaultValues();
            break;
		case KEY_DOWN:
			form_driver(form, REQ_NEXT_FIELD);
			form_driver(form, REQ_END_LINE);
			break;
        case KEY_STAB:
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
	free_field(fields[TRANSACTION_DATE_IDX * 2]);
	free_field(fields[TRANSACTION_DATE_IDX * 2 + 1]);
	free_field(fields[DESCRIPTION_IDX * 2]);
	free_field(fields[DESCRIPTION_IDX * 2 + 1]);
	free_field(fields[CATEGORY_IDX * 2]);
	free_field(fields[CATEGORY_IDX * 2 + 1]);
	free_field(fields[AMOUNT_IDX * 2]);
	free_field(fields[AMOUNT_IDX * 2 + 1]);
	free_field(fields[CLEARED_IDX * 2]);
	free_field(fields[CLEARED_IDX * 2 + 1]);
	free_field(fields[NOTES_IDX * 2]);
	free_field(fields[NOTES_IDX * 2 + 1]);
	free_field(fields[ACCOUNT_NAME_OWNER_IDX * 2]);
	free_field(fields[ACCOUNT_NAME_OWNER_IDX * 2 + 1]);
    free_field(fields[ACCOUNT_TYPE_IDX * 2]);
    free_field(fields[ACCOUNT_TYPE_IDX * 2 + 1]);
	delwin(win_form);
	delwin(win_body);
	endwin();
    win_form = NULL;
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
	mvwprintw(win_body, 1, 2, "Press ESC to quit; F2 to save; F12 to clear");

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

	assert(fields[0] != NULL && fields[1] != NULL && fields[2] != NULL && fields[3] != NULL);
	assert(fields[4] != NULL && fields[5] != NULL && fields[6] != NULL && fields[7] != NULL);
	assert(fields[8] != NULL && fields[9] != NULL);
	assert(fields[10] != NULL && fields[11] != NULL);
	assert(fields[12] != NULL && fields[13] != NULL);
    assert(fields[14] != NULL && fields[15] != NULL);

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

	while ((ch = wgetch(win_body)) != 27) { //escape = 27
		driver(ch);
    }

  cleanup_transaction_screen();
  show_main_screen();
}



void show_main_screen() {
    //int list_size = 6;
    char list[MAIN_MENU_LIST_SIZE][12] = { "transaction", "empty", "empty", "empty", "empty", "quit" };
    char item[12] = {0};
    int ch, i = 0;

    initscr(); // initialize Ncurses
    win_main_menu = newwin( 10, 15, 1, 1 ); // create a new window
    assert(win_main_menu != NULL);
    box(win_main_menu, 0, 0); // sets default borders for the window

// now print all the menu items and highlight the first one
    for( i = 0; i< MAIN_MENU_LIST_SIZE; i++ ) {
        if( i == 0 ) {
            wattron(win_main_menu, A_STANDOUT); // highlights the first item.
        } else {
            wattroff(win_main_menu, A_STANDOUT);
        }
        snprintf(item, sizeof(item), "%s", list[i]);
        mvwprintw(win_main_menu, i+1, 2, "%s", item);
    }

    wrefresh(win_main_menu); // update the terminal screen
    if( win_main_menu == NULL ) {
      printf("win_main_menu is null\n");
    }

    i = 0;
    noecho(); // disable echoing of characters on the screen
    keypad(win_main_menu, TRUE); // enable keyboard input for the window.
    curs_set(0); // hide the default screen cursor.

    ch = wgetch(win_main_menu);
    while( ch != 27 ) {
        snprintf(item, sizeof(item), "%s",  list[i]);
        mvwprintw(win_main_menu, i+1, 2, "%s", item );
        switch( ch ) {
            case KEY_UP:
            case 'k':
                i--;
                i = ( i < 0 ) ? (MAIN_MENU_LIST_SIZE-1) : i;
                break;
            case '\n':
                if( i == 0 ) {
                    delwin(win_main_menu);
                    endwin();
                    show_transaction_insert_screen();
                }

                if( i == 5 ) {
                  delwin(win_main_menu);
                  endwin();
                  exit(0);
                }
            break;
            case -1:
              exit(1);
            break;
            case KEY_DOWN:
            case 'j':
                i++;
                i = ( i>(MAIN_MENU_LIST_SIZE-1) ) ? 0 : i;
                break;
        }
        // now highlight the next item in the list.
        wattron(win_main_menu, A_STANDOUT);

        //sprintf(item, "%-7s",  list[i]);
        snprintf(item, sizeof(item), "%s",  list[i]);
        mvwprintw(win_main_menu, i+1, 2, "%s", item);
        wattroff(win_main_menu, A_STANDOUT);


        if( win_main_menu == NULL ) {
          printf("because the window pointer is null\n");
          exit(2);
        }
        ch = wgetch(win_main_menu);
    }

    if( ch == -1 ) {
      printf("exited application\n");
      exit(1);
    }
    delwin(win_main_menu);
    endwin();
    win_main_menu = NULL;
}

int main(int arg, char *argv[]) {
  show_main_screen();
  return 0;
}
