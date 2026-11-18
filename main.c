// To make the program work, open a new terminal and run these commands:
// gcc -o contact_manager contact_manager.c
// ./contact_manager
/*
=========================================================
   PROJECT 24: DYNAMIC CONTACT MANAGER & DEDUPLICATOR    
=========================================================

--- Dynamic Contact Manager ---
1. Add Contact
2. Display All Contacts
3. Edit Contact
4. Delete Contact
5. Detect & Merge Duplicates
6. Search Contacts (Prefix / Substring)
7. Export Contacts
8. Import Contacts
0. Exit
Choice: 8
Enter import filename (e.g., contacts.txt): contacts.txt
Imported 3 contacts from 'contacts.txt'.

Choice: 5

Duplicate detected:
  Primary:   John Doe
  Duplicate: john doe (Reason: [Name] [Phone] )
Merge into primary? (y/n): y
Merged successfully!

Choice: 6

Search Mode:
1. Prefix Match (Name)
2. Substring Match (Name, Phone, Email)
Choice: 2
Enter search term: 555

[1] Name: John Doe
    Phones: +1 (555) 234-5678, 555-999-0000
    Emails: john.doe@example.com, J.DOE@WORK.ORG
    Address: 42 Elm Street
    [Merged Record Linked -> Former Name: john doe]

Choice: 0
Memory freed. Exiting.
---------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_STR 100
#define MAX_ADDR 200

// Structure representing a single Contact
typedef struct Contact {
    char name[MAX_STR];
    char **phones;          // Dynamic array of strings for phone numbers
    int phoneCount;
    char **emails;          // Dynamic array of strings for emails
    int emailCount;
    char address[MAX_ADDR];
    struct Contact *mergedFrom; // Pointer to link historical/merged records
} Contact;

// Structure representing the Dynamic Contact List
typedef struct {
    Contact *list;          // Dynamic array of Contact structures
    int count;
    int capacity;
} ContactBook;

// --- Function Prototypes ---
void initBook(ContactBook *book);
void freeContact(Contact *c);
void freeBook(ContactBook *book);

// Dynamic Array Helpers
void addPhoneToContact(Contact *c, const char *phone);
void addEmailToContact(Contact *c, const char *email);

// Heuristics & String Utilities
void toLowerStr(const char *src, char *dest);
void normalizePhone(const char *src, char *dest);
int hasOverlappingPhone(const Contact *a, const Contact *b);
int hasOverlappingEmail(const Contact *a, const Contact *b);

// Core Operations
void addContactInteractive(ContactBook *book);
void displayContact(const Contact *c, int index);
void displayAll(const ContactBook *book);
void editContact(ContactBook *book);
void deleteContactAt(ContactBook *book, int index);
void mergeContacts(ContactBook *book, int targetIdx, int sourceIdx);
void detectAndMergeDuplicates(ContactBook *book);
void searchContacts(const ContactBook *book);
void exportContacts(const ContactBook *book, const char *filename);
void importContacts(ContactBook *book, const char *filename);

// --- Initialization & Cleanup ---

void initBook(ContactBook *book) {
    book->capacity = 4;
    book->count = 0;
    book->list = (Contact *)malloc(book->capacity * sizeof(Contact));
    if (!book->list) {
        perror("Failed to allocate initial contact book");
        exit(EXIT_FAILURE);
    }
}

void freeContact(Contact *c) {
    for (int i = 0; i < c->phoneCount; i++) {
        free(c->phones[i]);
    }
    free(c->phones);

    for (int i = 0; i < c->emailCount; i++) {
        free(c->emails[i]);
    }
    free(c->emails);

    // Free linked merged history if present
    if (c->mergedFrom != NULL) {
        freeContact(c->mergedFrom);
        free(c->mergedFrom);
    }
}

void freeBook(ContactBook *book) {
    for (int i = 0; i < book->count; i++) {
        freeContact(&book->list[i]);
    }
    free(book->list);
}

// --- Dynamic Array Helpers ---

void addPhoneToContact(Contact *c, const char *phone) {
    // Prevent adding exact duplicate phones to the same contact
    char normNew[MAX_STR], normExisting[MAX_STR];
    normalizePhone(phone, normNew);
    if (strlen(normNew) == 0) return;

    for (int i = 0; i < c->phoneCount; i++) {
        normalizePhone(c->phones[i], normExisting);
        if (strcmp(normNew, normExisting) == 0) return;
    }

    char **temp = (char **)realloc(c->phones, (c->phoneCount + 1) * sizeof(char *));
    if (!temp) {
        perror("Memory allocation failed for phones");
        return;
    }
    c->phones = temp;
    c->phones[c->phoneCount] = (char *)malloc(MAX_STR * sizeof(char));
    strncpy(c->phones[c->phoneCount], phone, MAX_STR - 1);
    c->phones[c->phoneCount][MAX_STR - 1] = '\0';
    c->phoneCount++;
}

void addEmailToContact(Contact *c, const char *email) {
    // Prevent adding duplicate emails (case-insensitive) to the same contact
    char lowerNew[MAX_STR], lowerExisting[MAX_STR];
    toLowerStr(email, lowerNew);
    if (strlen(lowerNew) == 0) return;

    for (int i = 0; i < c->emailCount; i++) {
        toLowerStr(c->emails[i], lowerExisting);
        if (strcmp(lowerNew, lowerExisting) == 0) return;
    }

    char **temp = (char **)realloc(c->emails, (c->emailCount + 1) * sizeof(char *));
    if (!temp) {
        perror("Memory allocation failed for emails");
        return;
    }
    c->emails = temp;
    c->emails[c->emailCount] = (char *)malloc(MAX_STR * sizeof(char));
    strncpy(c->emails[c->emailCount], email, MAX_STR - 1);
    c->emails[c->emailCount][MAX_STR - 1] = '\0';
    c->emailCount++;
}

// --- Heuristics & String Utilities ---

void toLowerStr(const char *src, char *dest) {
    int i = 0;
    while (src[i]) {
        dest[i] = tolower((unsigned char)src[i]);
        i++;
    }
    dest[i] = '\0';
}

// Heuristic: Strip all non-digit characters (spaces, dashes, parentheses, +)
void normalizePhone(const char *src, char *dest) {
    int j = 0;
    for (int i = 0; src[i] != '\0'; i++) {
        if (isdigit((unsigned char)src[i])) {
            dest[j++] = src[i];
        }
    }
    dest[j] = '\0';
}

int hasOverlappingPhone(const Contact *a, const Contact *b) {
    char normA[MAX_STR], normB[MAX_STR];
    for (int i = 0; i < a->phoneCount; i++) {
        normalizePhone(a->phones[i], normA);
        for (int j = 0; j < b->phoneCount; j++) {
            normalizePhone(b->phones[j], normB);
            if (strlen(normA) > 0 && strcmp(normA, normB) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int hasOverlappingEmail(const Contact *a, const Contact *b) {
    char lowA[MAX_STR], lowB[MAX_STR];
    for (int i = 0; i < a->emailCount; i++) {
        toLowerStr(a->emails[i], lowA);
        for (int j = 0; j < b->emailCount; j++) {
            toLowerStr(b->emails[j], lowB);
            if (strlen(lowA) > 0 && strcmp(lowA, lowB) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

// --- Core Operations ---

void ensureCapacity(ContactBook *book) {
    if (book->count >= book->capacity) {
        book->capacity *= 2;
        Contact *temp = (Contact *)realloc(book->list, book->capacity * sizeof(Contact));
        if (!temp) {
            perror("Failed to expand contact book");
            exit(EXIT_FAILURE);
        }
        book->list = temp;
    }
}

void readLine(char *buffer, int size) {
    if (fgets(buffer, size, stdin)) {
        buffer[strcspn(buffer, "\n")] = '\0';
    }
}

void addContactInteractive(ContactBook *book) {
    ensureCapacity(book);
    Contact *c = &book->list[book->count];
    memset(c, 0, sizeof(Contact));

    printf("Enter Name: ");
    readLine(c->name, MAX_STR);

    int numPhones;
    printf("How many phone numbers? ");
    scanf("%d", &numPhones);
    getchar(); // consume newline
    for (int i = 0; i < numPhones; i++) {
        char buf[MAX_STR];
        printf("  Phone %d: ", i + 1);
        readLine(buf, MAX_STR);
        addPhoneToContact(c, buf);
    }

    int numEmails;
    printf("How many emails? ");
    scanf("%d", &numEmails);
    getchar();
    for (int i = 0; i < numEmails; i++) {
        char buf[MAX_STR];
        printf("  Email %d: ", i + 1);
        readLine(buf, MAX_STR);
        addEmailToContact(c, buf);
    }

    printf("Enter Address: ");
    readLine(c->address, MAX_ADDR);

    book->count++;
    printf("Contact added successfully!\n");
}

void displayContact(const Contact *c, int index) {
    printf("\n[%d] Name: %s\n", index + 1, c->name);
    printf("    Phones: ");
    for (int i = 0; i < c->phoneCount; i++) {
        printf("%s%s", c->phones[i], (i < c->phoneCount - 1) ? ", " : "");
    }
    printf("\n    Emails: ");
    for (int i = 0; i < c->emailCount; i++) {
        printf("%s%s", c->emails[i], (i < c->emailCount - 1) ? ", " : "");
    }
    printf("\n    Address: %s\n", c->address);
    if (c->mergedFrom != NULL) {
        printf("    [Merged Record Linked -> Former Name: %s]\n", c->mergedFrom->name);
    }
}

void displayAll(const ContactBook *book) {
    if (book->count == 0) {
        printf("\nContact book is empty.\n");
        return;
    }
    printf("\n=== All Contacts (%d) ===", book->count);
    for (int i = 0; i < book->count; i++) {
        displayContact(&book->list[i], i);
    }
}

void editContact(ContactBook *book) {
    displayAll(book);
    if (book->count == 0) return;

    int idx;
    printf("\nEnter contact number to edit (1-%d): ", book->count);
    scanf("%d", &idx);
    getchar();
    idx--;

    if (idx < 0 || idx >= book->count) {
        printf("Invalid selection.\n");
        return;
    }

    Contact *c = &book->list[idx];
    char buf[MAX_ADDR];

    printf("New Name (leave blank to keep '%s'): ", c->name);
    readLine(buf, MAX_STR);
    if (strlen(buf) > 0) strcpy(c->name, buf);

    printf("Add a new Phone (leave blank to skip): ");
    readLine(buf, MAX_STR);
    if (strlen(buf) > 0) addPhoneToContact(c, buf);

    printf("Add a new Email (leave blank to skip): ");
    readLine(buf, MAX_STR);
    if (strlen(buf) > 0) addEmailToContact(c, buf);

    printf("New Address (leave blank to keep current): ");
    readLine(buf, MAX_ADDR);
    if (strlen(buf) > 0) strcpy(c->address, buf);

    printf("Contact updated.\n");
}

void deleteContactAt(ContactBook *book, int index) {
    if (index < 0 || index >= book->count) return;
    freeContact(&book->list[index]);

    // Shift remaining contacts left
    for (int i = index; i < book->count - 1; i++) {
        book->list[i] = book->list[i + 1];
    }
    book->count--;
}

// Merges sourceIdx into targetIdx and links a snapshot via pointers
void mergeContacts(ContactBook *book, int targetIdx, int sourceIdx) {
    Contact *target = &book->list[targetIdx];
    Contact *source = &book->list[sourceIdx];

    // 1. Archive a shallow/deep snapshot of the absorbed record via pointer
    Contact *archived = (Contact *)calloc(1, sizeof(Contact));
    strcpy(archived->name, source->name);
    strcpy(archived->address, source->address);
    archived->mergedFrom = target->mergedFrom; // Chain existing history
    target->mergedFrom = archived;

    // 2. Merge unique phones & emails
    for (int i = 0; i < source->phoneCount; i++) {
        addPhoneToContact(target, source->phones[i]);
    }
    for (int i = 0; i < source->emailCount; i++) {
        addEmailToContact(target, source->emails[i]);
    }

    // 3. Keep longer/non-empty address
    if (strlen(target->address) == 0 && strlen(source->address) > 0) {
        strcpy(target->address, source->address);
    }

    // 4. Remove duplicate entry from active array
    deleteContactAt(book, sourceIdx);
}

void detectAndMergeDuplicates(ContactBook *book) {
    int mergesPerformed = 0;

    for (int i = 0; i < book->count; i++) {
        for (int j = i + 1; j < book->count; j++) {
            char nameA[MAX_STR], nameB[MAX_STR];
            toLowerStr(book->list[i].name, nameA);
            toLowerStr(book->list[j].name, nameB);

            int sameName = (strcmp(nameA, nameB) == 0);
            int samePhone = hasOverlappingPhone(&book->list[i], &book->list[j]);
            int sameEmail = hasOverlappingEmail(&book->list[i], &book->list[j]);

            if (sameName || samePhone || sameEmail) {
                printf("\nDuplicate detected:\n");
                printf("  Primary:   %s\n", book->list[i].name);
                printf("  Duplicate: %s (Reason: %s%s%s)\n",
                       book->list[j].name,
                       sameName ? "[Name] " : "",
                       samePhone ? "[Phone] " : "",
                       sameEmail ? "[Email]" : "");

                printf("Merge into primary? (y/n): ");
                char choice;
                scanf(" %c", &choice);
                getchar();

                if (tolower(choice) == 'y') {
                    mergeContacts(book, i, j);
                    mergesPerformed++;
                    j--; // Re-evaluate current index after shift
                    printf("Merged successfully!\n");
                }
            }
        }
    }

    if (mergesPerformed == 0) {
        printf("\nNo duplicates merged.\n");
    }
}

void searchContacts(const ContactBook *book) {
    char query[MAX_STR], lowerQuery[MAX_STR], normQuery[MAX_STR];
    int mode;

    printf("\nSearch Mode:\n1. Prefix Match (Name)\n2. Substring Match (Name, Phone, Email)\nChoice: ");
    scanf("%d", &mode);
    getchar();

    printf("Enter search term: ");
    readLine(query, MAX_STR);
    toLowerStr(query, lowerQuery);
    normalizePhone(query, normQuery);

    int found = 0;
    for (int i = 0; i < book->count; i++) {
        const Contact *c = &book->list[i];
        char lowerName[MAX_STR];
        toLowerStr(c->name, lowerName);

        int match = 0;
        if (mode == 1) {
            // Prefix match on name
            if (strncmp(lowerName, lowerQuery, strlen(lowerQuery)) == 0) {
                match = 1;
            }
        } else {
            // Substring match across name, normalized phones, and lowercase emails
            if (strstr(lowerName, lowerQuery) != NULL) match = 1;

            if (!match && strlen(normQuery) > 0) {
                for (int p = 0; p < c->phoneCount; p++) {
                    char normP[MAX_STR];
                    normalizePhone(c->phones[p], normP);
                    if (strstr(normP, normQuery) != NULL) {
                        match = 1;
                        break;
                    }
                }
            }

            if (!match) {
                for (int e = 0; e < c->emailCount; e++) {
                    char lowE[MAX_STR];
                    toLowerStr(c->emails[e], lowE);
                    if (strstr(lowE, lowerQuery) != NULL) {
                        match = 1;
                        break;
                    }
                }
            }
        }

        if (match) {
            displayContact(c, i);
            found++;
        }
    }

    if (found == 0) printf("No matching contacts found.\n");
}

// --- File Import / Export (CSV-like format with '|' delimiter for multi-values) ---

void exportContacts(const ContactBook *book, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Could not open file for export");
        return;
    }

    for (int i = 0; i < book->count; i++) {
        const Contact *c = &book->list[i];
        fprintf(fp, "%s;", c->name);

        for (int p = 0; p < c->phoneCount; p++) {
            fprintf(fp, "%s%s", c->phones[p], (p < c->phoneCount - 1) ? "|" : "");
        }
        fprintf(fp, ";");

        for (int e = 0; e < c->emailCount; e++) {
            fprintf(fp, "%s%s", c->emails[e], (e < c->emailCount - 1) ? "|" : "");
        }
        fprintf(fp, ";%s\n", c->address);
    }

    fclose(fp);
    printf("Exported %d contacts to '%s'.\n", book->count, filename);
}

void importContacts(ContactBook *book, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Could not open file for import");
        return;
    }

    char line[512];
    int imported = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        ensureCapacity(book);
        Contact *c = &book->list[book->count];
        memset(c, 0, sizeof(Contact));

        // Format: Name;phone1|phone2;email1|email2;Address
        char *nameTok = strtok(line, ";");
        char *phonesTok = strtok(NULL, ";");
        char *emailsTok = strtok(NULL, ";");
        char *addrTok = strtok(NULL, ";");

        if (nameTok) strncpy(c->name, nameTok, MAX_STR - 1);
        if (addrTok) strncpy(c->address, addrTok, MAX_ADDR - 1);

        if (phonesTok) {
            char *p = strtok(phonesTok, "|");
            while (p) {
                addPhoneToContact(c, p);
                p = strtok(NULL, "|");
            }
        }

        if (emailsTok) {
            char *e = strtok(emailsTok, "|");
            while (e) {
                addEmailToContact(c, e);
                e = strtok(NULL, "|");
            }
        }

        book->count++;
        imported++;
    }

    fclose(fp);
    printf("Imported %d contacts from '%s'.\n", imported, filename);
}

// --- Main Menu ---

int main() {
    ContactBook book;
    initBook(&book);

    int choice;
    char filename[MAX_STR];

    do {
        printf("\n--- Dynamic Contact Manager ---\n");
        printf("1. Add Contact\n");
        printf("2. Display All Contacts\n");
        printf("3. Edit Contact\n");
        printf("4. Delete Contact\n");
        printf("5. Detect & Merge Duplicates\n");
        printf("6. Search Contacts (Prefix / Substring)\n");
        printf("7. Export Contacts\n");
        printf("8. Import Contacts\n");
        printf("0. Exit\n");
        printf("Choice: ");
        scanf("%d", &choice);
        getchar();

        switch (choice) {
            case 1: addContactInteractive(&book); break;
            case 2: displayAll(&book); break;
            case 3: editContact(&book); break;
            case 4: {
                displayAll(&book);
                if (book.count > 0) {
                    int idx;
                    printf("Enter contact number to delete (1-%d): ", book.count);
                    scanf("%d", &idx);
                    getchar();
                    deleteContactAt(&book, idx - 1);
                    printf("Contact deleted.\n");
                }
                break;
            }
            case 5: detectAndMergeDuplicates(&book); break;
            case 6: searchContacts(&book); break;
            case 7:
                printf("Enter export filename (e.g., contacts.txt): ");
                readLine(filename, MAX_STR);
                exportContacts(&book, filename);
                break;
            case 8:
                printf("Enter import filename (e.g., contacts.txt): ");
                readLine(filename, MAX_STR);
                importContacts(&book, filename);
                break;
            case 0:
                freeBook(&book);
                printf("Memory freed. Exiting.\n");
                break;
            default:
                printf("Invalid choice.\n");
        }
    } while (choice != 0);

    return 0;
}
