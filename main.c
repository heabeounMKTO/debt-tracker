
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

#define MAX_NAME_LEN 128 
#define MAX_DESC_LEN 1024 
#define MAX_DATE_LEN 48



typedef enum {
  DEBT_ACTIVE=0,
  DEBT_PAID=1,
  DEBT_DEFAULTED=2
} DebtStatus;



const char* status_str[] = {"Active", "Paid Off", "Gone"};
typedef  struct {
  int id;
  char debtor_name[MAX_NAME_LEN];
  double amount;
  char description[MAX_DESC_LEN];
  char date_created[MAX_DATE_LEN];
  char date_due[MAX_DATE_LEN];
  DebtStatus status;
} Debt;


sqlite3 *db = NULL;


int init_database();
void cleanup_database();
int add_debt();
int list_debts();
int update_debt_status();
int delete_debt();
void get_current_date(char* buffer);
void print_debt(const Debt* debt);
void clear_input_buffer();
int get_menu_choice();


int main() {
  if (init_database() != SQLITE_OK) {
    fprintf(stderr, "failed to initilize databse\n");
    return -1;
  } 
      
    printf("=== DEBT TRACKER ===\n");
    printf("Keep track of who owes you money!\n\n");
    
    int choice;
    do {
        printf("\n--- MAIN MENU ---\n");
        printf("1. Add new debt\n");
        printf("2. List all debts\n");
        printf("3. Update debt status\n");
        printf("4. Delete debt\n");
        printf("5. Exit\n");
        printf("Enter your choice (1-5): ");
        
        choice = get_menu_choice();
        
        switch (choice) {
            case 1:
                add_debt();
                break;
            case 2:
                list_debts();
                break;
            case 3:
                update_debt_status();
                break;
            case 4:
                delete_debt();
                break;
            case 5:
                printf("Goodbye!\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 5);
    
    cleanup_database();
    return 0;
}




int init_database() {
    int rc = sqlite3_open("debts.db", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    const char* create_table_sql = 
        "CREATE TABLE IF NOT EXISTS debts ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "debtor_name TEXT NOT NULL,"
        "amount REAL NOT NULL,"
        "description TEXT,"
        "date_created TEXT NOT NULL,"
        "date_due TEXT,"
        "status INTEGER DEFAULT 0"
        ");";
    
    char* err_msg = NULL;
    rc = sqlite3_exec(db, create_table_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return rc;
    }
    
    printf("Database initialized successfully.\n");
    return SQLITE_OK;
}

void cleanup_database() {
    if (db) {
        sqlite3_close(db);
    }
}

int add_debt() {
    Debt new_debt = {0};
    
    printf("\n--- ADD NEW DEBT ---\n");
    
    printf("Debtor's name: ");
    fgets(new_debt.debtor_name, MAX_NAME_LEN, stdin);
    new_debt.debtor_name[strcspn(new_debt.debtor_name, "\n")] = 0; // Remove newline
    
    printf("Amount owed: $");
    scanf("%lf", &new_debt.amount);
    clear_input_buffer();
    
    printf("Description (optional): ");
    fgets(new_debt.description, MAX_DESC_LEN, stdin);
    new_debt.description[strcspn(new_debt.description, "\n")] = 0;
    
    get_current_date(new_debt.date_created);
    
    printf("Due date (YYYY-MM-DD) or press Enter for no due date: ");
    fgets(new_debt.date_due, MAX_DATE_LEN, stdin);
    new_debt.date_due[strcspn(new_debt.date_due, "\n")] = 0;
    
    new_debt.status = DEBT_ACTIVE;
    
    const char* insert_sql = 
        "INSERT INTO debts (debtor_name, amount, description, date_created, date_due, status) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    sqlite3_bind_text(stmt, 1, new_debt.debtor_name, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 2, new_debt.amount);
    sqlite3_bind_text(stmt, 3, new_debt.description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, new_debt.date_created, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, new_debt.date_due, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, new_debt.status);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        printf("Debt added successfully!\n");
    } else {
        fprintf(stderr, "Failed to insert debt: %s\n", sqlite3_errmsg(db));
    }
    
    sqlite3_finalize(stmt);
    return rc;
}

int list_debts() {
    printf("\n--- ALL DEBTS ---\n");
    
    const char* select_sql = 
        "SELECT id, debtor_name, amount, description, date_created, date_due, status "
        "FROM debts ORDER BY date_created DESC;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, select_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    int count = 0;
    double total_active = 0.0;
    
    printf("%-4s %-20s %-10s %-15s %-12s %-12s %s\n", 
           "ID", "Debtor", "Amount", "Status", "Created", "Due", "Description");
    printf("------------------------------------------------------------------------------------\n");
    
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Debt debt;
        debt.id = sqlite3_column_int(stmt, 0);
        strncpy(debt.debtor_name, (char*)sqlite3_column_text(stmt, 1), MAX_NAME_LEN);
        debt.amount = sqlite3_column_double(stmt, 2);
        strncpy(debt.description, (char*)sqlite3_column_text(stmt, 3), MAX_DESC_LEN);
        strncpy(debt.date_created, (char*)sqlite3_column_text(stmt, 4), MAX_DATE_LEN);
        strncpy(debt.date_due, (char*)sqlite3_column_text(stmt, 5), MAX_DATE_LEN);
        debt.status = sqlite3_column_int(stmt, 6);
        
        printf("%-4d %-20.20s $%-9.2f %-15s %-12s %-12s %s\n",
               debt.id, debt.debtor_name, debt.amount, 
               status_str[debt.status], debt.date_created, 
               debt.date_due, debt.description);
        
        if (debt.status == DEBT_ACTIVE) {
            total_active += debt.amount;
        }
        count++;
    }
    
    if (count == 0) {
        printf("No debts found.\n");
    } else {
        printf("\nTotal debts: %d\n", count);
        printf("Total active debt amount: $%.2f\n", total_active);
    }
    
    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

int update_debt_status() {
    int debt_id;
    int new_status;
    
    printf("\n--- UPDATE DEBT STATUS ---\n");
    
    // First, show current debts
    list_debts();
    
    printf("\nEnter debt ID to update: ");
    scanf("%d", &debt_id);
    clear_input_buffer();
    
    printf("New status:\n");
    printf("0 - Active\n");
    printf("1 - Paid Off\n");
    printf("2 - Defaulted\n");
    printf("Enter status (0-2): ");
    scanf("%d", &new_status);
    clear_input_buffer();
    
    if (new_status < 0 || new_status > 2) {
        printf("Invalid status!\n");
        return 1;
    }
    
    const char* update_sql = "UPDATE debts SET status = ? WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    sqlite3_bind_int(stmt, 1, new_status);
    sqlite3_bind_int(stmt, 2, debt_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        if (sqlite3_changes(db) > 0) {
            printf("Debt status updated to: %s\n", status_str[new_status]);
        } else {
            printf("No debt found with ID %d\n", debt_id);
        }
    } else {
        fprintf(stderr, "Failed to update debt: %s\n", sqlite3_errmsg(db));
    }
    
    sqlite3_finalize(stmt);
    return rc;
}

int delete_debt() {
    int debt_id;
    
    printf("\n--- DELETE DEBT ---\n");
    
    // Show current debts
    list_debts();
    
    printf("\nEnter debt ID to delete: ");
    scanf("%d", &debt_id);
    clear_input_buffer();
    
    printf("Are you sure you want to delete this debt? (y/N): ");
    char confirm;
    scanf("%c", &confirm);
    clear_input_buffer();
    
    if (confirm != 'y' && confirm != 'Y') {
        printf("Deletion cancelled.\n");
        return 0;
    }
    
    const char* delete_sql = "DELETE FROM debts WHERE id = ?;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, delete_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }
    
    sqlite3_bind_int(stmt, 1, debt_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        if (sqlite3_changes(db) > 0) {
            printf("Debt deleted successfully!\n");
        } else {
            printf("No debt found with ID %d\n", debt_id);
        }
    } else {
        fprintf(stderr, "Failed to delete debt: %s\n", sqlite3_errmsg(db));
    }
    
    sqlite3_finalize(stmt);
    return rc;
}

void get_current_date(char* buffer) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, MAX_DATE_LEN, "%Y-%m-%d", tm_info);
}

int get_menu_choice() {
    int choice;
    if (scanf("%d", &choice) != 1) {
        clear_input_buffer();
        return -1;
    }
    clear_input_buffer();
    return choice;
}

void clear_input_buffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}
