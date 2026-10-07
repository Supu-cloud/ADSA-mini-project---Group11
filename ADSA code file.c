#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024
#define MAX_FILES 128
#define MAX_FILENAME 64
#define MAX_CONTENT 4096
#define T 2
#define MAX_KEYS (2 * T - 1)
#define MAX_CHILD (2 * T)
//Group 11 
//Disk structure
typedef struct {
    int id;
    int failed;
    char *blocks[MAX_BLOCKS];  //Array of pointers to blocks
} Disk;

//meta data structure
typedef struct {
    int allocated;
    int ref_count; 
} BlockMeta;
/*
//file meta data 
typedef struct {
    char name[MAX_FILENAME];
    int size;
    int blocks[MAX_BLOCKS];
    int block_count;
    int in_use;
} FileMeta;

//B tree data structure 
typedef struct BTreeNode {
    int is_leaf;
    int n_keys;
    char keys[MAX_KEYS][MAX_FILENAME];
    int file_indices[MAX_KEYS];
    struct BTreeNode *children[MAX_CHILD];
} BTreeNode;

typedef struct {
    Disk disk1, disk2;
    FileMeta files[MAX_FILES]; //Array of metadata for files
    BlockMeta block_meta[MAX_BLOCKS];
    int next_block_id;
    BTreeNode *btree_root;
} Btrfs;
*/
static char *str_dup_local(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = malloc(n);

    if (!p) {
        perror("malloc");
        exit(1);
    }

    memcpy(p, s, n);
    return p;
}

void disk_init(Disk *d, int id)
{
    d->id = id;
    d->failed = 0;

    for (int i = 0; i < MAX_BLOCKS; i++) {
        d->blocks[i] = NULL;
    }
}

void disk_free_all(Disk *d)
{
    for (int i = 0; i < MAX_BLOCKS; i++) {
        free(d->blocks[i]);
        d->blocks[i] = NULL;
    }
}

int disk_write(Disk *d, int id, const char *data)
{
    if (id < 0 || id >= MAX_BLOCKS || d->failed) {
        return 0;
    }

    free(d->blocks[id]);
    d->blocks[id] = str_dup_local(data);
    return 1;
}

const char *disk_read(Disk *d, int id)
{
    if (id < 0 || id >= MAX_BLOCKS || d->failed) {
        return NULL;
    }

    return d->blocks[id];
}

void disk_free_block(Disk *d, int id)
{
    if (id >= 0 && id < MAX_BLOCKS) {
        free(d->blocks[id]);
        d->blocks[id] = NULL;
    }
}

BTreeNode *btree_create_node(int leaf)
{
    BTreeNode *n = malloc(sizeof(*n));

    if (!n) {
        perror("malloc");
        exit(1);
    }

    n->is_leaf = leaf;
    n->n_keys = 0;

    for (int i = 0; i < MAX_CHILD; i++) {
        n->children[i] = NULL;
    }

    return n;
}

void btree_free(BTreeNode *n)
{
    if (!n) {
        return;
    }

    if (!n->is_leaf) {
        for (int i = 0; i <= n->n_keys; i++) {
            btree_free(n->children[i]);
        }
    }

    free(n);
}
//Btree function
void btree_split_child(BTreeNode *x, int i, BTreeNode *y)
{
    BTreeNode *z = btree_create_node(y->is_leaf);
    z->n_keys = T - 1;

    for (int j = 0; j < T - 1; j++) {
        strcpy(z->keys[j], y->keys[j + T]);
        z->file_indices[j] = y->file_indices[j + T];
    }

    if (!y->is_leaf) {
        for (int j = 0; j < T; j++) {
            z->children[j] = y->children[j + T];
        }
    }

    y->n_keys = T - 1;

    for (int j = x->n_keys; j >= i + 1; j--) {
        x->children[j + 1] = x->children[j];
    }

    x->children[i + 1] = z;

    for (int j = x->n_keys - 1; j >= i; j--) {
        strcpy(x->keys[j + 1], x->keys[j]);
        x->file_indices[j + 1] = x->file_indices[j];
    }

    strcpy(x->keys[i], y->keys[T - 1]);
    x->file_indices[i] = y->file_indices[T - 1];
    x->n_keys++;
}

//Btree function

void btree_insert_nonfull(BTreeNode *x, const char *k, int idx)
{
    int i = x->n_keys - 1;

    if (x->is_leaf) {
        while (i >= 0 && strcmp(k, x->keys[i]) < 0) { //String function
            strcpy(x->keys[i + 1], x->keys[i]);
            x->file_indices[i + 1] = x->file_indices[i];
            i--;
        }

        strcpy(x->keys[i + 1], k);
        x->file_indices[i + 1] = idx;
        x->n_keys++;
    } else {
        while (i >= 0 && strcmp(k, x->keys[i]) < 0) {
            i--;
        }

        i++;

        if (x->children[i]->n_keys == MAX_KEYS) {
            btree_split_child(x, i, x->children[i]);
            if (strcmp(k, x->keys[i]) > 0) {
                i++;
            }
        }

        btree_insert_nonfull(x->children[i], k, idx);
    }
}
//Btree function
BTreeNode *btree_insert(BTreeNode *r, const char *k, int idx)
{
    if (!r) {
        r = btree_create_node(1);
        strcpy(r->keys[0], k);
        r->file_indices[0] = idx;
        r->n_keys = 1;
        return r;
    }

    if (r->n_keys == MAX_KEYS) {
        BTreeNode *nr = btree_create_node(0);
        nr->children[0] = r;
        btree_split_child(nr, 0, r);
        btree_insert_nonfull(nr, k, idx);
        return nr;
    }

    btree_insert_nonfull(r, k, idx);
    return r;
}
//Btree search function
int btree_search(BTreeNode *r, const char *k)
{
    if (!r) {
        return -1;
    }

    int i = 0;

    while (i < r->n_keys && strcmp(k, r->keys[i]) > 0) {
        i++;
    }

    if (i < r->n_keys && strcmp(k, r->keys[i]) == 0) {
        return r->file_indices[i];
    }

    if (r->is_leaf) {
        return -1;
    }

    return btree_search(r->children[i], k);
}

static int btree_find_key(BTreeNode *n, const char *k)
{
    for (int i = 0; i < n->n_keys; i++) {
        if (strcmp(n->keys[i], k) == 0) {
            return i;
        }
    }

    return -1;
}

static void btree_get_predecessor(BTreeNode *n, int idx, char *k, int *v)
{
    BTreeNode *c = n->children[idx];

    while (!c->is_leaf) {
        c = c->children[c->n_keys];
    }

    strcpy(k, c->keys[c->n_keys - 1]);
    *v = c->file_indices[c->n_keys - 1];
}

static void btree_get_successor(BTreeNode *n, int idx, char *k, int *v)
{
    BTreeNode *c = n->children[idx + 1];

    while (!c->is_leaf) {
        c = c->children[0];
    }

    strcpy(k, c->keys[0]);
    *v = c->file_indices[0];
}

static void btree_borrow_from_prev(BTreeNode *n, int idx)
{
    BTreeNode *c = n->children[idx];
    BTreeNode *s = n->children[idx - 1];

    for (int i = c->n_keys - 1; i >= 0; i--) {
        strcpy(c->keys[i + 1], c->keys[i]);
        c->file_indices[i + 1] = c->file_indices[i];
    }

    if (!c->is_leaf) {
        for (int i = c->n_keys; i >= 0; i--) {
            c->children[i + 1] = c->children[i];
        }
    }

    strcpy(c->keys[0], n->keys[idx - 1]);
    c->file_indices[0] = n->file_indices[idx - 1];

    if (!c->is_leaf) {
        c->children[0] = s->children[s->n_keys];
    }

    strcpy(n->keys[idx - 1], s->keys[s->n_keys - 1]);
    n->file_indices[idx - 1] = s->file_indices[s->n_keys - 1];
    c->n_keys++;
    s->n_keys--;
}

static void btree_borrow_from_next(BTreeNode *n, int idx)
{
    BTreeNode *c = n->children[idx];
    BTreeNode *s = n->children[idx + 1];

    strcpy(c->keys[c->n_keys], n->keys[idx]);
    c->file_indices[c->n_keys] = n->file_indices[idx];

    if (!c->is_leaf) {
        c->children[c->n_keys + 1] = s->children[0];
    }

    strcpy(n->keys[idx], s->keys[0]);
    n->file_indices[idx] = s->file_indices[0];

    for (int i = 1; i < s->n_keys; i++) {
        strcpy(s->keys[i - 1], s->keys[i]);
        s->file_indices[i - 1] = s->file_indices[i];
    }

    if (!s->is_leaf) {
        for (int i = 1; i <= s->n_keys; i++) {
            s->children[i - 1] = s->children[i];
        }
    }

    c->n_keys++;
    s->n_keys--;
}

static void btree_merge_children(BTreeNode *n, int idx)
{
    BTreeNode *c = n->children[idx];
    BTreeNode *s = n->children[idx + 1];

    strcpy(c->keys[T - 1], n->keys[idx]);
    c->file_indices[T - 1] = n->file_indices[idx];

    for (int i = 0; i < s->n_keys; i++) {
        strcpy(c->keys[i + T], s->keys[i]);
        c->file_indices[i + T] = s->file_indices[i];
    }

    if (!c->is_leaf) {
        for (int i = 0; i <= s->n_keys; i++) {
            c->children[i + T] = s->children[i];
        }
    }

    for (int i = idx + 1; i < n->n_keys; i++) {
        strcpy(n->keys[i - 1], n->keys[i]);
        n->file_indices[i - 1] = n->file_indices[i];
    }

    for (int i = idx + 2; i <= n->n_keys; i++) {
        n->children[i - 1] = n->children[i];
    }

    c->n_keys += s->n_keys + 1;
    n->n_keys--;
    free(s);
}

static void btree_fill(BTreeNode *n, int idx)
{
    if (idx != 0 && n->children[idx - 1]->n_keys >= T) {
        btree_borrow_from_prev(n, idx);
    } else if (idx != n->n_keys && n->children[idx + 1]->n_keys >= T) {
        btree_borrow_from_next(n, idx);
    } else if (idx != n->n_keys) {
        btree_merge_children(n, idx);
    } else {
        btree_merge_children(n, idx - 1);
    }
}
//Btree delete function
static void btree_delete_nonroot(BTreeNode *n, const char *k)
{
    int idx = btree_find_key(n, k);

    if (idx != -1) {
        if (n->is_leaf) {
            for (int i = idx + 1; i < n->n_keys; i++) {
                strcpy(n->keys[i - 1], n->keys[i]);
                n->file_indices[i - 1] = n->file_indices[i];
            }
            n->n_keys--;
        } else if (n->children[idx]->n_keys >= T) {
            char pk[MAX_FILENAME];
            int pv;

            btree_get_predecessor(n, idx, pk, &pv);
            strcpy(n->keys[idx], pk);
            n->file_indices[idx] = pv;
            btree_delete_nonroot(n->children[idx], pk);
        } else if (n->children[idx + 1]->n_keys >= T) {
            char sk[MAX_FILENAME];
            int sv;

            btree_get_successor(n, idx, sk, &sv);
            strcpy(n->keys[idx], sk);
            n->file_indices[idx] = sv;
            btree_delete_nonroot(n->children[idx + 1], sk);
        } else {
            btree_merge_children(n, idx);
            btree_delete_nonroot(n->children[idx], k);
        }

        return;
    }

    if (n->is_leaf) {
        return;
    }

    int i = 0;

    while (i < n->n_keys && strcmp(k, n->keys[i]) > 0) {
        i++;
    }

    if (n->children[i]->n_keys < T) {
        btree_fill(n, i);
    }

    if (i > n->n_keys) {
        btree_delete_nonroot(n->children[i - 1], k);
    } else {
        btree_delete_nonroot(n->children[i], k);
    }
}

BTreeNode *btree_delete(BTreeNode *r, const char *k)
{
    if (!r) {
        return NULL;
    }

    if (btree_search(r, k) < 0) {
        return r;
    }

    btree_delete_nonroot(r, k);

    if (r->n_keys == 0) {
        BTreeNode *nr = r->is_leaf ? NULL : r->children[0];
        free(r);
        return nr;
    }

    return r;
}

void btree_inorder(BTreeNode *r, void (*visit)(const char *, int, void *), void *ctx)
{
    if (!r) {
        return;
    }

    for (int i = 0; i < r->n_keys; i++) {
        if (!r->is_leaf) {
            btree_inorder(r->children[i], visit, ctx);
        }
        visit(r->keys[i], r->file_indices[i], ctx);
    }

    if (!r->is_leaf) {
        btree_inorder(r->children[r->n_keys], visit, ctx);
    }
}

void fs_init(Btrfs *fs)
{
    disk_init(&fs->disk1, 1);
    disk_init(&fs->disk2, 2);

    fs->next_block_id = 1;
    fs->btree_root = NULL;

    for (int i = 0; i < MAX_FILES; i++) {
        fs->files[i].in_use = 0;
    }

    for (int i = 0; i < MAX_BLOCKS; i++) {
        fs->block_meta[i].allocated = 0;
        fs->block_meta[i].ref_count = 0;
    }
}
//Block allocation
static int fs_allocate_block(Btrfs *fs)
{
    for (int i = 1; i < fs->next_block_id && i < MAX_BLOCKS; i++) {
        if (!fs->block_meta[i].allocated) {
            fs->block_meta[i].allocated = 1;
            fs->block_meta[i].ref_count = 1;
            return i;
        }
    }

    if (fs->next_block_id >= MAX_BLOCKS) {
        return -1;
    }

    int id = fs->next_block_id++;
    fs->block_meta[id].allocated = 1;
    fs->block_meta[id].ref_count = 1;
    return id;
}

static int fs_write_block(Btrfs *fs, const char *data)
{
    int id = fs_allocate_block(fs);
    if (id < 0) {
        return -1;
    }

    int ok = 0;

    //RAID1 
    if (!fs->disk1.failed) {
        ok |= disk_write(&fs->disk1, id, data);
    }

    if (!fs->disk2.failed) {
        ok |= disk_write(&fs->disk2, id, data);
    }

    if (!ok) {
        fs->block_meta[id].allocated = 0;
        fs->block_meta[id].ref_count = 0;
        return -1;
    }

    return id;
}

static int fs_share_block(Btrfs *fs, int id)
{
    if (id < 0 || id >= MAX_BLOCKS || !fs->block_meta[id].allocated) {
        return 0;
    }

    fs->block_meta[id].ref_count++;
    return 1;
}

static void fs_release_block(Btrfs *fs, int id)
{
    if (id < 0 || id >= MAX_BLOCKS || !fs->block_meta[id].allocated) {
        return;
    }

    if (--fs->block_meta[id].ref_count <= 0) {
        disk_free_block(&fs->disk1, id);
        disk_free_block(&fs->disk2, id);
        fs->block_meta[id].allocated = 0;
        fs->block_meta[id].ref_count = 0;
    }
}

static const char *fs_read_block(Btrfs *fs, int id)
{
    const char *d = disk_read(&fs->disk1, id);

    if (!d) {
        d = disk_read(&fs->disk2, id);
    }

    return d;
}
//Divide files in to blocks
static int fs_store_content(Btrfs *fs, FileMeta *f, const char *content)
{
    int total = (int)strlen(content);
    int count = 0;

    if (total == 0) {
        int id = fs_write_block(fs, "");

        if (id < 0) {
            return 0;
        }

        f->blocks[count++] = id;
    } else {
        for (int off = 0; off < total;) {
            if (count >= MAX_BLOCKS) {
                return 0;
            }

            int chunk = (total - off) > BLOCK_SIZE ? BLOCK_SIZE : (total - off);
            char buf[BLOCK_SIZE + 1];

            memcpy(buf, content + off, chunk);
            buf[chunk] = '\0';

            int id = fs_write_block(fs, buf);

            if (id < 0) {
                return 0;
            }

            f->blocks[count++] = id;
            off += chunk;
        }
    }

    f->block_count = count;
    f->size = total;
    return 1;
}/*
//File creation function
void fs_create(Btrfs *fs, const char *name, const char *content)
{
    if (btree_search(fs->btree_root, name) >= 0) {
        printf("[ERROR] File '%s' already exists.\n", name);
        return;
    }

    int slot = -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs->files[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        printf("[ERROR] Filesystem full.\n");  //error handling
        return;
    }

    FileMeta *f = &fs->files[slot];
    memset(f, 0, sizeof(*f));
    strncpy(f->name, name, MAX_FILENAME - 1);
    f->in_use = 1;

    if (!fs_store_content(fs, f, content)) {
        f->in_use = 0;
        printf("[ERROR] Not enough block space or both disks unavailable.\n");
        return;
    }

    fs->btree_root = btree_insert(fs->btree_root, name, slot);

    printf("[CREATE] '%s' size=%dB, blocks=%d (RAID 1 + CoW-ready)\n",
           name, f->size, f->block_count);
}
//Copy On Write Function (COW)
void fs_copy(Btrfs *fs, const char *src, const char *dst)
{
    int si = btree_search(fs->btree_root, src);

    if (si < 0) {
        printf("[ERROR] Source '%s' not found.\n", src);
        return;
    }

    if (btree_search(fs->btree_root, dst) >= 0) {
        printf("[ERROR] Destination '%s' already exists.\n", dst);
        return;
    }

    int slot = -1;

    for (int i = 0; i < MAX_FILES; i++) {
        if (!fs->files[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        printf("[ERROR] Filesystem full.\n");
        return;
    }

    FileMeta *f = &fs->files[slot];
    FileMeta *s = &fs->files[si];

    memset(f, 0, sizeof(*f));
    strncpy(f->name, dst, MAX_FILENAME - 1);
    f->size = s->size;
    f->block_count = s->block_count;
    f->in_use = 1;

    for (int i = 0; i < s->block_count; i++) {
        f->blocks[i] = s->blocks[i];
        fs_share_block(fs, f->blocks[i]);
    }

    fs->btree_root = btree_insert(fs->btree_root, dst, slot);

    printf("[COPY/CoW] '%s' -> '%s' (shared %d blocks; no immediate data copy)\n",
           src, dst, f->block_count);
}

void fs_read(Btrfs *fs, const char *name)
{
    int idx = btree_search(fs->btree_root, name);

    if (idx < 0) {
        printf("[ERROR] File '%s' not found.\n", name);
        return;
    }

    FileMeta *f = &fs->files[idx];
    char buf[MAX_CONTENT];
    int pos = 0;

    for (int i = 0; i < f->block_count; i++) {
        const char *d = fs_read_block(fs, f->blocks[i]);  //file read

        if (!d) {
            printf("[ERROR] Block %d unavailable on both disks.\n", f->blocks[i]);
            return;
        }

        int len = (int)strlen(d);

        if (pos + len >= MAX_CONTENT) {
            len = MAX_CONTENT - 1 - pos;
        }

        memcpy(buf + pos, d, len);
        pos += len;
        buf[pos] = '\0';

        if (pos >= MAX_CONTENT - 1) {
            break;
        }
    }

    printf("[READ] '%s' -> \"%s\"\n", name, buf);
}
//file deletion
void fs_delete(Btrfs *fs, const char *name)
{
    int idx = btree_search(fs->btree_root, name);

    if (idx < 0) {
        printf("[ERROR] File '%s' not found.\n", name);
        return;
    }

    FileMeta *f = &fs->files[idx];
    int n = f->block_count;

    for (int i = 0; i < n; i++) {
        fs_release_block(fs, f->blocks[i]);
    }

    fs->btree_root = btree_delete(fs->btree_root, name);
    f->in_use = 0;
    f->block_count = 0;

    printf("[DELETE] '%s' removed; %d block references released\n", name, n);
}

void fs_modify(Btrfs *fs, const char *name, const char *new_content)
{
    int idx = btree_search(fs->btree_root, name);

    if (idx < 0) {
        printf("[ERROR] File '%s' not found.\n", name);
        return;
    }

    FileMeta *f = &fs->files[idx];
    int total = (int)strlen(new_content);
    int new_count = 0;
    int new_blocks[MAX_BLOCKS];
    int off = 0;

    while (1) {
        int chunk = (total - off) > BLOCK_SIZE ? BLOCK_SIZE : (total - off);
        char buf[BLOCK_SIZE + 1];

        if (total == 0) {
            chunk = 0;
        }

        if (chunk > 0) {
            memcpy(buf, new_content + off, chunk);
            buf[chunk] = '\0';
        } else {
            buf[0] = '\0';
        }

        int reused = 0;

        if (new_count < f->block_count) {
            const char *old = fs_read_block(fs, f->blocks[new_count]);

            if (old && strcmp(old, buf) == 0) {
                new_blocks[new_count] = f->blocks[new_count];
                reused = 1;
            }
        }

        if (!reused) {
            int id = fs_write_block(fs, buf);

            if (id < 0) {
                for (int j = 0; j < new_count; j++) {
                    if (new_blocks[j] != f->blocks[j]) {
                        fs_release_block(fs, new_blocks[j]);
                    }
                }

                printf("[ERROR] Modification failed: no writable disk space.\n");
                return;
            }

            new_blocks[new_count] = id;
        }

        new_count++;

        if (total == 0 || off + chunk >= total) {
            break;
        }

        off += chunk;

        if (new_count >= MAX_BLOCKS) {
            printf("[ERROR] File exceeds block limit.\n");
            return;
        }
    }

    int old_count = f->block_count;
    int old_blocks[MAX_BLOCKS];

    for (int i = 0; i < old_count; i++) {
        old_blocks[i] = f->blocks[i];
    }

    for (int i = 0; i < new_count; i++) {
        f->blocks[i] = new_blocks[i];
    }

    f->block_count = new_count;
    f->size = total;

    for (int i = 0; i < old_count; i++) {
        int retained = 0;

        for (int j = 0; j < new_count; j++) {
            if (new_blocks[j] == old_blocks[i]) {
                retained = 1;
                break;
            }
        }

        if (!retained) {
            fs_release_block(fs, old_blocks[i]);
        }
    }

    printf("[MODIFY/CoW] '%s' -> new size %dB, blocks=%d\n",
           name, f->size, f->block_count);
}

void fs_fail_disk(Btrfs *fs, int id)
{
    Disk *d = id == 1 ? &fs->disk1 : &fs->disk2;

    if (d->failed) {
        printf("[INFO] Disk %d is already failed.\n", id);
        return;
    }

    if (fs->disk1.failed && fs->disk2.failed) {
        printf("[ERROR] Both disks are already failed.\n");
        return;
    }

    d->failed = 1;
    printf("[FAIL] Disk %d marked as FAILED. RAID 1 can read from the surviving disk.\n", id);
}
//Disk repair function
int fs_repair_disk(Btrfs *fs, int id)
{
    Disk *target = id == 1 ? &fs->disk1 : &fs->disk2;
    Disk *source = id == 1 ? &fs->disk2 : &fs->disk1;

    if (!target->failed) {
        printf("[INFO] Disk %d is not failed.\n", id);
        return 0;
    }

    if (source->failed) {
        printf("[ERROR] Cannot repair Disk %d because both disks are failed.\n", id);
        return 0;
    }

    target->failed = 0;
    disk_free_all(target);

    int count = 0;

    for (int b = 0; b < MAX_BLOCKS; b++) {
        if (!fs->block_meta[b].allocated) {
            continue;
        }

        const char *d = disk_read(source, b);

        if (!d) {
            printf("[ERROR] Source disk missing allocated block %d. Repair aborted.\n", b);
            return 0;
        }

        if (!disk_write(target, b, d)) {
            printf("[ERROR] Failed to rebuild block %d.\n", b);
            return 0;
        }

        count++;
    }

    printf("[REPAIR] Disk %d rebuilt successfully from Disk %d (%d blocks).\n",
           id, source->id, count);
    return 1;
}

void fs_status(Btrfs *fs)
{
    printf("\n--- SYSTEM STATUS ---\n");
    printf("Disk 1: %s\n", fs->disk1.failed ? "FAILED" : "ONLINE");
    printf("Disk 2: %s\n", fs->disk2.failed ? "FAILED" : "ONLINE");

    int used = 0;

    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (fs->block_meta[i].allocated) {
            used++;
        }
    }

    printf("Allocated blocks: %d/%d\n", used, MAX_BLOCKS);
    printf("---------------------\n");
}

static void btree_visit_list(const char *name, int idx, void *ctx)
{
    Btrfs *fs = ctx;

    if (idx < 0 || idx >= MAX_FILES || !fs->files[idx].in_use) {
        return;
    }

    printf("  %-20s %10d %8d\n", name, fs->files[idx].size, fs->files[idx].block_count);
}
//File listing
void fs_list(Btrfs *fs)
{
    printf("  %-20s %10s %8s\n", "Name", "Size(B)", "Blocks");
    printf("  -----------------------------------------\n");

    if (!fs->btree_root) {
        printf("  (no files)\n");
        return;
    }

    btree_inorder(fs->btree_root, btree_visit_list, fs);
}

void fs_block_info(Btrfs *fs, const char *name)
{
    int idx = btree_search(fs->btree_root, name);

    if (idx < 0) {
        printf("[ERROR] File '%s' not found.\n", name);
        return;
    }

    FileMeta *f = &fs->files[idx];
    printf("[BLOCKS] %s: ", name);

    for (int i = 0; i < f->block_count; i++) {
        printf("%d(ref=%d)%s",
               f->blocks[i],
               fs->block_meta[f->blocks[i]].ref_count,
               i + 1 == f->block_count ? "\n" : ", ");
    }
}

void fs_shutdown(Btrfs *fs)
{
    disk_free_all(&fs->disk1);
    disk_free_all(&fs->disk2);
    btree_free(fs->btree_root);
    fs->btree_root = NULL;
}

void trim(char *s)
{
    size_t n = strlen(s);

    if (n && s[n - 1] == '\n') {
        s[n - 1] = '\0';
    }
}

void print_menu(void)
{
    printf("\n=========================================\n");
    printf("Btrfs-Inspired File System Simulator\n\n");
    printf("1. List files\n");
    printf("2. Create file\n");
    printf("3. Read file\n");
    printf("4. Copy file (CoW)\n");
    printf("5. Delete file\n");
    printf("6. Modify file (CoW)\n");
    printf("7. Fail Disk 1\n");
    printf("8. Fail Disk 2\n");
    printf("9. Repair Disk 1\n");
    printf("10. Repair Disk 2\n");
    printf("11. Show system status\n");
    printf("12. Show file block/reference info\n");
    printf("0. Exit\n");
    printf("=========================================\n");
}
*/
int main(void)
{
    Btrfs fs;
    fs_init(&fs);

    char line[64];
    char name[MAX_FILENAME];
    char content[MAX_CONTENT];

    while (1) { //menu interface
        print_menu();
        printf("Choose option: ");

        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        int choice = atoi(line);

        switch (choice) {
            case 1:
                fs_list(&fs);
                break;

            case 2:
                printf("Filename: ");
                fgets(name, sizeof(name), stdin);
                trim(name);

                printf("Content : ");
                fgets(content, sizeof(content), stdin);
                trim(content);

                fs_create(&fs, name, content); //File creation
                break;

            case 3:
                printf("Filename: ");
                fgets(name, sizeof(name), stdin);
                trim(name);
                fs_read(&fs, name);
                break;

            case 4: {
                char dst[MAX_FILENAME];

                printf("Source      : ");
                fgets(name, sizeof(name), stdin);
                trim(name);

                printf("Destination : ");
                fgets(dst, sizeof(dst), stdin);
                trim(dst);

                fs_copy(&fs, name, dst);
                break;
            }

            case 5:
                printf("Filename to delete: ");
                fgets(name, sizeof(name), stdin);
                trim(name);
                fs_delete(&fs, name);
                break;

            case 6:
                printf("Filename to modify: ");
                fgets(name, sizeof(name), stdin);
                trim(name);

                printf("New content       : ");
                fgets(content, sizeof(content), stdin);
                trim(content);

                fs_modify(&fs, name, content);
                break;

            case 7:
                fs_fail_disk(&fs, 1);
                break;

            case 8:
                fs_fail_disk(&fs, 2);
                break;

            case 9:
                fs_repair_disk(&fs, 1);
                break;

            case 10:
                fs_repair_disk(&fs, 2);
                break;

            case 11:
                fs_status(&fs);
                break;

            case 12:
                printf("Filename: ");
                fgets(name, sizeof(name), stdin);
                trim(name);
                fs_block_info(&fs, name);
                break;

            case 0:
                printf("Goodbye.\n");
                fs_shutdown(&fs);
                return 0;

            default:
                printf("[ERROR] Invalid choice.\n");
        }
    }

    fs_shutdown(&fs);
    return 0;
}