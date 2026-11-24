#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
using namespace std;

//using custom dynamic array so i can make any type of array for any type 
template <typename t>
class dynarr {
    t* arr;
    int cap;
    int sz;

    //doubles the storage when the array is full
    void resize(int newcap) {
        t* tmp = new t[newcap];
        for (int i = 0; i < sz; i++) tmp[i] = arr[i];
        delete[] arr;
        arr = tmp;
        cap = newcap;
    }

public:
    dynarr(
        int initcap = 8
    ) : arr(nullptr),
        cap(initcap > 0 ? initcap : 1),
        sz(0) {
        arr = new t[cap];
    }

    ~dynarr() {
        delete[] arr;
    }

    dynarr(
        const dynarr& other
    ) : arr(nullptr),
        cap(other.cap),
        sz(other.sz) {
        arr = new t[cap];
        for (int i = 0; i < sz; i++) arr[i] = other.arr[i];
    }

    dynarr& operator=(const dynarr& other) {
        if (this != &other) {
            delete[] arr;
            cap = other.cap;
            sz = other.sz;
            arr = new t[cap];
            for (int i = 0; i < sz; i++) arr[i] = other.arr[i];
        }
        return *this;
    }

    void push(const t& val) {
        if (sz >= cap) resize(cap * 2);
        arr[sz++] = val;
    }

    int size() const { return sz; }
    void clear() { sz = 0; }
    t& operator[](int idx) { return arr[idx]; }
    const t& operator[](int idx) const { return arr[idx]; }
};

//one disk block inside a linked list
struct blknode {
    int id;
    blknode* next;

    blknode(
        int bnum,
        blknode* nxt = nullptr
    ) : id(bnum),
        next(nxt) {}
};

//makes a copy of a block list and counts its nodes
blknode* clonelist(blknode* src, int& count) {
    blknode* head = nullptr;
    blknode* tail = nullptr;
    count = 0;
    while (src != nullptr) {
        blknode* nd = new blknode(src->id);
        if (head == nullptr) head = nd;
        else tail->next = nd;
        tail = nd;
        count++;
        src = src->next;
    }
    return head;
}

//frees every node of a block list
void deletelist(blknode*& head) {
    while (head != nullptr) {
        blknode* tmp = head;
        head = head->next;
        delete tmp;
    }
}

//simulated disk that hands out blocks from a free list
class disk {
    int tot;
    int bsz;
    int freecnt;
    blknode* freehead;

    void clearfree() {
        deletelist(freehead);
        freecnt = 0;
    }

public:
    disk(
        int numblocks = 24,
        int blockbytes = 32
    ) : tot(numblocks),
        bsz(blockbytes),
        freecnt(0),
        freehead(nullptr) {
        reset(numblocks, blockbytes);
    }

    ~disk() { clearfree(); }

    disk(
        const disk& other
    ) : tot(other.tot),
        bsz(other.bsz),
        freecnt(0),
        freehead(nullptr) {
        freehead = clonelist(other.freehead, freecnt);
    }

    disk& operator=(const disk& other) {
        if (this != &other) {
            clearfree();
            tot = other.tot;
            bsz = other.bsz;
            freehead = clonelist(other.freehead, freecnt);
        }
        return *this;
    }

    //rebuilds the free list so block 0 is at the front
    void reset(int numblocks, int blockbytes) {
        clearfree();
        tot = numblocks;
        bsz = blockbytes;
        for (int i = numblocks - 1; i >= 0; i--) {
            freehead = new blknode(i, freehead);
            freecnt++;
        }
    }

    //every file needs at least 1 block
    int needblocks(int bytes) const {
        if (bytes <= 0) return 1;
        return (bytes + bsz - 1) / bsz;
    }

    //takes k blocks from the free list, fails if the disk is full
    bool alloc(int k, blknode*& outhead, int& got) {
        outhead = nullptr;
        got = 0;
        if (k > freecnt) return false;
        blknode* tail = nullptr;
        for (int i = 0; i < k; i++) {
            blknode* b = freehead;
            freehead = freehead->next;
            freecnt--;
            b->next = nullptr;
            if (outhead == nullptr) outhead = b;
            else tail->next = b;
            tail = b;
            got++;
        }
        return true;
    }

    //gives a file's blocks back to the free list
    void release(blknode*& head) {
        while (head != nullptr) {
            blknode* tmp = head;
            head = head->next;
            tmp->next = freehead;
            freehead = tmp;
            freecnt++;
        }
    }

    int freeblocks() const { return freecnt; }
    int usedblocks() const { return tot - freecnt; }
    int totalblocks() const { return tot; }
    int blocksize() const { return bsz; }
};

//kmp substring search, used by the find command
bool kmpsearch(const string& txt, const string& pat) {
    if (pat.empty()) return true;
    int n = (int)txt.size();
    int m = (int)pat.size();
    if (m > n) return false;

    //build the table of longest prefix that is also a suffix
    int* lps = new int[m];
    lps[0] = 0;
    int len = 0;
    int i = 1;
    while (i < m) {
        if (pat[i] == pat[len]) lps[i++] = ++len;
        else if (len != 0) len = lps[len - 1];
        else lps[i++] = 0;
    }

    //scan the text using the table to skip repeated work
    i = 0;
    int j = 0;
    bool matched = false;
    while (i < n) {
        if (txt[i] == pat[j]) {
            i++;
            j++;
            if (j == m) {
                matched = true;
                break;
            }
        } else if (j != 0) j = lps[j - 1];
        else i++;
    }
    delete[] lps;
    return matched;
}

struct fsnode;

//one entry inside a hash table chain
struct hbucket {
    string key;
    fsnode* child;
    hbucket* next;

    hbucket(
        const string& k,
        fsnode* c,
        hbucket* nxt = nullptr
    ) : key(k),
        child(c),
        next(nxt) {}
};

//a file or a folder, folders keep their children in a hash table
struct fsnode {
    string nm;
    string content;
    bool isdir;
    bool hidden;
    bool locked;
    string passcode;
    blknode* blocks;
    int blkcnt;
    fsnode* parent;
    hbucket** table;
    int cap;
    int cnt;

    //djb2 hash turned into a bucket index
    static int hashstr(const string& s, int m) {
        unsigned long h = 5381;
        for (int i = 0; i < (int)s.size(); i++) h = ((h << 5) + h) + (unsigned char)s[i];
        return (int)(h % (unsigned long)m);
    }

    void inittable(int c = 8) {
        cap = (c < 4) ? 4 : c;
        table = new hbucket*[cap];
        cnt = 0;
        for (int i = 0; i < cap; i++) table[i] = nullptr;
    }

    void freeblocks() {
        deletelist(blocks);
        blkcnt = 0;
    }

    //deletes all children recursively and the table itself
    void clearchildren() {
        if (table == nullptr) return;
        for (int i = 0; i < cap; i++) {
            hbucket* cur = table[i];
            while (cur != nullptr) {
                hbucket* tmp = cur;
                cur = cur->next;
                delete tmp->child;
                delete tmp;
            }
        }
        delete[] table;
        table = nullptr;
        cap = 0;
        cnt = 0;
    }

    //deep copies every child of another folder
    void copychildren(const fsnode& other) {
        if (!isdir || other.table == nullptr) return;
        inittable(other.cap);
        for (int i = 0; i < other.cap; i++) {
            for (hbucket* c = other.table[i]; c != nullptr; c = c->next) {
                fsnode* cloned = new fsnode(*(c->child));
                cloned->parent = this;
                int idx = hashstr(c->key, cap);
                table[idx] = new hbucket(c->key, cloned, table[idx]);
                cnt++;
            }
        }
    }

    //doubles the table when it gets too full
    void rehash() {
        int oldcap = cap;
        hbucket** oldtab = table;
        inittable(oldcap * 2);
        for (int i = 0; i < oldcap; i++) {
            hbucket* cur = oldtab[i];
            while (cur != nullptr) {
                int idx = hashstr(cur->key, cap);
                table[idx] = new hbucket(cur->key, cur->child, table[idx]);
                cnt++;
                hbucket* tmp = cur;
                cur = cur->next;
                delete tmp;
            }
        }
        delete[] oldtab;
    }

    fsnode(
        const string& nameval = "",
        bool dirflag = false,
        fsnode* parptr = nullptr
    ) : nm(nameval),
        content(""),
        isdir(dirflag),
        hidden(false),
        locked(false),
        passcode(""),
        blocks(nullptr),
        blkcnt(0),
        parent(parptr),
        table(nullptr),
        cap(0),
        cnt(0) {
        if (isdir) inittable(8);
    }

    ~fsnode() {
        freeblocks();
        clearchildren();
    }

    fsnode(
        const fsnode& other
    ) : nm(other.nm),
        content(other.content),
        isdir(other.isdir),
        hidden(other.hidden),
        locked(other.locked),
        passcode(other.passcode),
        blocks(nullptr),
        blkcnt(0),
        parent(nullptr),
        table(nullptr),
        cap(0),
        cnt(0) {
        blocks = clonelist(other.blocks, blkcnt);
        copychildren(other);
    }

    fsnode& operator=(const fsnode& other) {
        if (this != &other) {
            freeblocks();
            clearchildren();
            nm = other.nm;
            content = other.content;
            isdir = other.isdir;
            hidden = other.hidden;
            locked = other.locked;
            passcode = other.passcode;
            blocks = clonelist(other.blocks, blkcnt);
            copychildren(other);
        }
        return *this;
    }

    //looks up a child by name in the hash table
    fsnode* findchild(const string& target) const {
        if (!isdir || table == nullptr) return nullptr;
        for (hbucket* cur = table[hashstr(target, cap)]; cur != nullptr; cur = cur->next) {
            if (cur->key == target) return cur->child;
        }
        return nullptr;
    }

    //adds a child, duplicate names are not allowed
    bool addchild(fsnode* ch) {
        if (!isdir || ch == nullptr || findchild(ch->nm) != nullptr) return false;
        if ((double)(cnt + 1) / (double)cap > 0.75) rehash();
        ch->parent = this;
        int idx = hashstr(ch->nm, cap);
        table[idx] = new hbucket(ch->nm, ch, table[idx]);
        cnt++;
        return true;
    }

    //detaches a child from the table without deleting it
    bool removechild(const string& target, fsnode*& outnode) {
        outnode = nullptr;
        if (!isdir || table == nullptr) return false;
        int idx = hashstr(target, cap);
        hbucket* cur = table[idx];
        hbucket* prev = nullptr;
        while (cur != nullptr) {
            if (cur->key == target) {
                if (prev != nullptr) prev->next = cur->next;
                else table[idx] = cur->next;
                outnode = cur->child;
                outnode->parent = nullptr;
                delete cur;
                cnt--;
                return true;
            }
            prev = cur;
            cur = cur->next;
        }
        return false;
    }

    //puts every child pointer into one array
    void getchildren(dynarr<fsnode*>& out) const {
        out.clear();
        if (!isdir || table == nullptr) return;
        for (int i = 0; i < cap; i++) {
            for (hbucket* cur = table[i]; cur != nullptr; cur = cur->next) out.push(cur->child);
        }
    }

    //size of a file, or the total size of everything inside a folder
    int sizebytes() const {
        if (!isdir) return (int)content.size();
        dynarr<fsnode*> kids;
        getchildren(kids);
        int total = 0;
        for (int i = 0; i < kids.size(); i++) total += kids[i]->sizebytes();
        return total;
    }
};

//merge sort for ls output, sorts by name or by size
void mergesort(dynarr<fsnode*>& arr, int left, int right, bool bysize) {
    if (left >= right) return;
    int mid = left + (right - left) / 2;
    mergesort(arr, left, mid, bysize);
    mergesort(arr, mid + 1, right, bysize);

    //copy both halves into temporary buffers
    int n1 = mid - left + 1;
    int n2 = right - mid;
    fsnode** lbuf = new fsnode*[n1];
    fsnode** rbuf = new fsnode*[n2];
    for (int i = 0; i < n1; i++) lbuf[i] = arr[left + i];
    for (int j = 0; j < n2; j++) rbuf[j] = arr[mid + 1 + j];

    //merge them back in order
    int i = 0;
    int j = 0;
    int k = left;
    while (i < n1 && j < n2) {
        bool pickleft = bysize
            ? (lbuf[i]->sizebytes() <= rbuf[j]->sizebytes())
            : (lbuf[i]->nm <= rbuf[j]->nm);
        arr[k++] = pickleft ? lbuf[i++] : rbuf[j++];
    }
    while (i < n1) arr[k++] = lbuf[i++];
    while (j < n2) arr[k++] = rbuf[j++];
    delete[] lbuf;
    delete[] rbuf;
}

//stack node for the popd command
struct stknode {
    fsnode* dir;
    stknode* next;

    stknode(
        fsnode* dptr,
        stknode* nxt = nullptr
    ) : dir(dptr),
        next(nxt) {}
};

//linked stack that remembers previous directories
class dirstack {
    stknode* top;

public:
    dirstack(
    ) : top(nullptr) {}

    ~dirstack() {
        while (top != nullptr) {
            stknode* tmp = top;
            top = top->next;
            delete tmp;
        }
    }

    void push(fsnode* d) { top = new stknode(d, top); }
    bool empty() const { return top == nullptr; }

    fsnode* pop() {
        if (top == nullptr) return nullptr;
        fsnode* res = top->dir;
        stknode* tmp = top;
        top = top->next;
        delete tmp;
        return res;
    }
};

//doubly linked list node for the command history
struct histnode {
    string cmd;
    histnode* prev;
    histnode* next;

    histnode(
        const string& cstr
    ) : cmd(cstr),
        prev(nullptr),
        next(nullptr) {}
};

//keeps every command the player typed
class cmdhistory {
    histnode* head;
    histnode* tail;

public:
    cmdhistory(
    ) : head(nullptr),
        tail(nullptr) {}

    ~cmdhistory() {
        while (head != nullptr) {
            histnode* tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    //adds the newest command at the tail
    void record(const string& c) {
        histnode* nd = new histnode(c);
        if (head == nullptr) head = nd;
        else {
            tail->next = nd;
            nd->prev = tail;
        }
        tail = nd;
    }

    void print() const {
        int num = 1;
        for (histnode* cur = head; cur != nullptr; cur = cur->next) {
            cout << "  " << num++ << "  " << cur->cmd << "\n";
        }
    }
};

//main game controller, holds the file system and runs the commands
class gameengine {
    fsnode* root;
    fsnode* cwd;
    disk storage;
    dirstack dirstk;
    cmdhistory hist;
    string goalpath;
    int moves;
    int score;

    void err(const string& msg) { cout << msg << "\n"; }

    //builds the full path of a node by walking up to the root
    string fullpath(fsnode* node) const {
        if (node == nullptr || node == root) return "/";
        string p = fullpath(node->parent);
        return (p == "/") ? "/" + node->nm : p + "/" + node->nm;
    }

    //walks the path one segment at a time, handles . and ..
    fsnode* resolverec(fsnode* cur, const string& path, int pos) const {
        if (cur == nullptr) return nullptr;
        while (pos < (int)path.size() && path[pos] == '/') pos++;
        if (pos >= (int)path.size()) return cur;

        int slash = (int)path.find('/', pos);
        int end = (slash == -1) ? (int)path.size() : slash;
        string seg = path.substr(pos, end - pos);

        if (seg == ".") return resolverec(cur, path, end + 1);
        if (seg == "..") return resolverec(cur->parent != nullptr ? cur->parent : cur, path, end + 1);
        if (!cur->isdir) return nullptr;
        return resolverec(cur->findchild(seg), path, end + 1);
    }

    //absolute paths start at root, relative ones at cwd
    fsnode* resolve(const string& p) const {
        if (p.empty()) return cwd;
        if (p[0] == '/') return resolverec(root, p, 1);
        return resolverec(cwd, p, 0);
    }

    //splits a path into parent path and last name
    void splitpath(const string& p, string& parp, string& nm) const {
        int sl = (int)p.rfind('/');
        parp = (sl == -1) ? "" : (sl == 0 ? "/" : p.substr(0, sl));
        nm = (sl == -1) ? p : p.substr(sl + 1);
    }

    //creates a file and gives it disk blocks, returns null when the disk is full
    fsnode* makefile(const string& name, const string& text, fsnode* par) {
        blknode* b = nullptr;
        int got = 0;
        if (!storage.alloc(storage.needblocks((int)text.size()), b, got)) return nullptr;
        fsnode* f = new fsnode(name, false, par);
        f->content = text;
        f->blocks = b;
        f->blkcnt = got;
        return f;
    }

    //returns the disk blocks of a file or of everything inside a folder
    void releasesubtree(fsnode* node) {
        if (node == nullptr) return;
        if (!node->isdir) {
            storage.release(node->blocks);
            node->blkcnt = 0;
            return;
        }
        dynarr<fsnode*> kids;
        node->getchildren(kids);
        for (int i = 0; i < kids.size(); i++) releasesubtree(kids[i]);
    }

    //prints the folder structure with indentation, hidden items are skipped
    void showtree(fsnode* node, int depth) const {
        for (int i = 0; i < depth; i++) cout << "  ";
        cout << (node->isdir ? "[DIR] " : "[FILE] ") << node->nm
             << (node->locked ? " [LOCKED]" : "") << "\n";
        if (!node->isdir) return;

        dynarr<fsnode*> kids;
        node->getchildren(kids);
        if (kids.size() > 1) mergesort(kids, 0, kids.size() - 1, false);
        for (int i = 0; i < kids.size(); i++) {
            if (!kids[i]->hidden) showtree(kids[i], depth + 1);
        }
    }

    //dfs with backtracking, outpath keeps the route to the goal file
    bool dfshint(fsnode* cur, dynarr<string>& outpath) const {
        if (cur == nullptr) return false;
        string here = fullpath(cur);
        outpath.push(here);
        if (here == goalpath) return true;
        if (cur->isdir) {
            dynarr<fsnode*> kids;
            cur->getchildren(kids);
            for (int i = 0; i < kids.size(); i++) {
                if (dfshint(kids[i], outpath)) return true;
            }
        }
        return false;
    }

public:
    gameengine(
    ) : root(nullptr),
        cwd(nullptr),
        storage(24, 32),
        goalpath("/home/vault/key.txt"),
        moves(0),
        score(100) {
        defaultlevel();
    }

    ~gameengine() { delete root; }

    //built in puzzle: unlock the vault and read the hidden key
    void defaultlevel() {
        delete root;
        storage.reset(24, 32);
        root = new fsnode("/", true, nullptr);

        fsnode* home = new fsnode("home", true, root);
        fsnode* vault = new fsnode("vault", true, home);
        vault->locked = true;
        vault->passcode = "1234";
        fsnode* note = makefile("hint.txt", "Unlock vault with 1234 and read key.txt", home);
        fsnode* secret = makefile("key.txt", "ESCAPE_COMPLETE", vault);
        secret->hidden = true;

        home->addchild(note);
        home->addchild(vault);
        vault->addchild(secret);
        root->addchild(home);
        cwd = home;
        goalpath = "/home/vault/key.txt";
        moves = 0;
        score = 100;
    }

    //reads a level text file line by line, falls back to the default level
    void loadlevel(const string& filename) {
        ifstream fin(filename);
        if (!fin) {
            err("Level file not found, loading default level.");
            defaultlevel();
            return;
        }
        delete root;
        root = new fsnode("/", true, nullptr);
        cwd = root;
        moves = 0;
        score = 100;

        string line;
        while (getline(fin, line)) {
            if (line.empty() || line[0] == '#') continue;
            istringstream iss(line);
            string tag;
            iss >> tag;

            if (tag == "DISK") {
                int tb = 24;
                int bs = 32;
                iss >> tb >> bs;
                storage.reset(tb, bs);
            } else if (tag == "GOAL") {
                iss >> goalpath;
            } else if (tag == "START") {
                string p;
                iss >> p;
                fsnode* n = resolve(p);
                if (n != nullptr && n->isdir) cwd = n;
            } else if (tag == "DIR" || tag == "FILE") {
                //both lines start with a path, FILE then has a hidden flag and the text
                string p;
                string code = "";
                string txt = "";
                int flag = 0;
                iss >> p >> flag;
                if (tag == "DIR") iss >> code;
                else {
                    getline(iss, txt);
                    if (!txt.empty() && txt[0] == ' ') txt.erase(0, 1);
                }

                string parp;
                string nm;
                splitpath(p, parp, nm);
                fsnode* par = parp.empty() ? cwd : resolve(parp);
                if (par == nullptr || !par->isdir || nm.empty()) continue;

                fsnode* nd = (tag == "DIR") ? new fsnode(nm, true, par) : makefile(nm, txt, par);
                if (nd == nullptr) continue;
                if (tag == "DIR") {
                    nd->locked = (flag != 0);
                    nd->passcode = code;
                } else nd->hidden = (flag != 0);
                par->addchild(nd);
            }
        }
        if (cwd == root && root->findchild("home") != nullptr) cwd = root->findchild("home");
    }

    //runs one shell command typed by the player
    void execcmd(const string& line) {
        if (line.empty()) return;
        hist.record(line);
        moves++;
        istringstream iss(line);
        string cmd;
        iss >> cmd;

        if (cmd == "pwd") {
            cout << fullpath(cwd) << "\n";
        } else if (cmd == "ls") {
            //-s sorts by size, -a also shows hidden files
            string opt;
            bool bysize = false;
            bool showall = false;
            while (iss >> opt) {
                if (opt == "-s") bysize = true;
                else if (opt == "-a") showall = true;
            }
            dynarr<fsnode*> kids;
            cwd->getchildren(kids);
            if (kids.size() > 1) mergesort(kids, 0, kids.size() - 1, bysize);
            for (int i = 0; i < kids.size(); i++) {
                if (!showall && kids[i]->hidden) continue;
                cout << (kids[i]->isdir ? "DIR  " : "FILE ") << kids[i]->nm
                     << " (" << kids[i]->sizebytes() << "B)"
                     << (kids[i]->locked ? " [LOCKED]" : "") << "\n";
            }
        } else if (cmd == "cd") {
            string target;
            if (!(iss >> target)) target = "/home";
            fsnode* dest = resolve(target);
            if (dest == nullptr) return err("cd: no such directory: " + target);
            if (!dest->isdir) return err("cd: not a directory: " + target);
            if (dest->locked) return err("cd: permission denied (locked folder): " + target);
            dirstk.push(cwd);
            cwd = dest;
        } else if (cmd == "popd") {
            if (dirstk.empty()) return err("popd: directory stack empty");
            cwd = dirstk.pop();
            cout << fullpath(cwd) << "\n";
        } else if (cmd == "unlock") {
            string target;
            string code;
            if (!(iss >> target >> code)) return err("unlock: usage unlock <dir> <code>");
            fsnode* d = resolve(target);
            if (d == nullptr || !d->isdir) return err("unlock: directory not found");
            if (!d->locked) return err("unlock: already unlocked");
            if (d->passcode.empty() || d->passcode == code) {
                d->locked = false;
                cout << "unlocked " << d->nm << "\n";
            } else err("unlock: wrong passcode");
        } else if (cmd == "mkdir") {
            string name;
            if (!(iss >> name)) return err("mkdir: missing directory name");
            fsnode* nd = new fsnode(name, true, cwd);
            if (!cwd->addchild(nd)) {
                delete nd;
                err("mkdir: cannot create '" + name + "' (already exists)");
            }
        } else if (cmd == "touch") {
            string name;
            if (!(iss >> name)) return err("touch: missing file name");
            if (cwd->findchild(name) != nullptr) return err("touch: file '" + name + "' already exists");
            fsnode* f = makefile(name, "", cwd);
            if (f == nullptr) return err("touch: disk full! Delete files with rm to free blocks.");
            cwd->addchild(f);
        } else if (cmd == "write") {
            string name;
            string text;
            if (!(iss >> name)) return err("write: missing file name");
            getline(iss, text);
            if (!text.empty() && text[0] == ' ') text.erase(0, 1);

            //new file: allocate blocks and attach it
            fsnode* f = cwd->findchild(name);
            if (f == nullptr) {
                f = makefile(name, text, cwd);
                if (f == nullptr) return err("write: disk full! Cannot write file.");
                cwd->addchild(f);
                return;
            }
            if (f->isdir) return err("write: '" + name + "' is a directory");

            //existing file: grow the block chain only if the new text needs more
            int extra = storage.needblocks((int)text.size()) - f->blkcnt;
            if (extra > 0) {
                blknode* nb = nullptr;
                int got = 0;
                if (!storage.alloc(extra, nb, got)) return err("write: disk full! Cannot expand file.");
                if (f->blocks == nullptr) f->blocks = nb;
                else {
                    blknode* cur = f->blocks;
                    while (cur->next != nullptr) cur = cur->next;
                    cur->next = nb;
                }
                f->blkcnt += got;
            }
            f->content = text;
        } else if (cmd == "cat") {
            string name;
            if (!(iss >> name)) return err("cat: missing file name");
            fsnode* f = cwd->findchild(name);
            if (f == nullptr) return err("cat: file not found: " + name);
            if (f->isdir) return err("cat: " + name + " is a directory");
            cout << f->content << "\n";

            //reading the goal file wins the game, fewer moves give a better score
            if (fullpath(f) == goalpath) {
                score = (100 - moves * 2 > 10) ? (100 - moves * 2) : 10;
                cout << "\n=== GOAL REACHED! Moves: " << moves << " | Score: " << score << " ===\n";
            }
        } else if (cmd == "rm") {
            string a1;
            string a2;
            bool rec = false;
            if (!(iss >> a1)) return err("rm: missing target");
            if (a1 == "-r") {
                rec = true;
                if (!(iss >> a2)) return err("rm -r: missing target");
            } else a2 = a1;

            fsnode* target = cwd->findchild(a2);
            if (target == nullptr) return err("rm: cannot remove '" + a2 + "': no such entry");
            if (target->isdir && !rec) return err("rm: cannot remove '" + a2 + "': is a directory (use rm -r)");

            //detach, free its disk blocks, then delete the memory
            fsnode* detached = nullptr;
            if (cwd->removechild(a2, detached)) {
                releasesubtree(detached);
                delete detached;
            }
        } else if (cmd == "mv") {
            string src;
            string dst;
            if (!(iss >> src >> dst)) return err("mv: usage mv <src> <dst>");
            if (cwd->findchild(src) == nullptr) return err("mv: source '" + src + "' not found");

            fsnode* dnode = resolve(dst);
            fsnode* det = nullptr;
            if (dnode != nullptr && dnode->isdir) {
                //destination is a folder, so move the item into it
                if (dnode->findchild(src) != nullptr) return err("mv: target directory already has '" + src + "'");
                if (cwd->removechild(src, det)) dnode->addchild(det);
            } else {
                //otherwise rename it inside the current folder
                if (cwd->findchild(dst) != nullptr) return err("mv: name '" + dst + "' already exists");
                if (cwd->removechild(src, det)) {
                    det->nm = dst;
                    cwd->addchild(det);
                }
            }
        } else if (cmd == "tree") {
            showtree(cwd, 0);
        } else if (cmd == "du") {
            int b = cwd->sizebytes();
            cout << "Size: " << b << " bytes (" << storage.needblocks(b) << " blocks)\n";
        } else if (cmd == "df") {
            cout << "Disk: " << storage.usedblocks() << "/" << storage.totalblocks()
                 << " blocks used | Free: " << storage.freeblocks()
                 << " | Block size: " << storage.blocksize() << "B\n";
        } else if (cmd == "find") {
            string pat;
            if (!(iss >> pat)) return err("find: usage find <substring>");

            //bfs using the dynamic array as a queue
            dynarr<fsnode*> queue;
            queue.push(cwd);
            for (int front = 0; front < queue.size(); front++) {
                fsnode* cur = queue[front];
                if (kmpsearch(cur->nm, pat)) cout << fullpath(cur) << "\n";
                dynarr<fsnode*> kids;
                cur->getchildren(kids);
                for (int i = 0; i < kids.size(); i++) queue.push(kids[i]);
            }
        } else if (cmd == "hint") {
            dynarr<string> route;
            if (dfshint(root, route) && route.size() > 0) {
                cout << "Hint path: ";
                for (int i = 0; i < route.size(); i++) cout << route[i] << (i + 1 < route.size() ? " -> " : "\n");
            } else err("No hint path found.");
        } else if (cmd == "history") {
            hist.print();
        } else if (cmd == "help") {
            cout << "Commands: pwd, ls [-a] [-s], cd <path>, popd, unlock <dir> <code>, "
                 << "mkdir <name>, touch <name>, write <file> <text>, cat <file>, "
                 << "rm [-r] <target>, mv <src> <dst>, tree, du, df, find <pat>, "
                 << "hint, history, exit\n";
        } else {
            err("Unknown command '" + cmd + "'. Type 'help'.");
        }
    }

    //quick edge case checks, type test inside the shell
    void runtests() {
        cout << "Running edge-case tests...\n";
        defaultlevel();

        cout << "[Test 1: cd .. at root]\n";
        execcmd("cd /");
        execcmd("cd ..");
        execcmd("pwd");

        cout << "[Test 2: duplicate names]\n";
        execcmd("cd /home");
        execcmd("mkdir dupdir");
        execcmd("mkdir dupdir");

        cout << "[Test 3: deep path]\n";
        execcmd("mkdir d1");
        execcmd("cd d1");
        execcmd("mkdir d2");
        execcmd("cd d2");
        execcmd("mkdir d3");
        execcmd("cd d3");
        execcmd("pwd");

        //30 touches on a 24 block disk must hit the disk full message
        cout << "[Test 4: disk full trap]\n";
        for (int i = 0; i < 30; i++) {
            ostringstream os;
            os << "touch blk" << i;
            execcmd(os.str());
        }
        execcmd("df");
        cout << "Edge-case tests finished.\n";
    }

    //main loop: show prompt, read a line, run it
    void runshell(const string& levelfile) {
        if (!levelfile.empty()) loadlevel(levelfile);
        string line;
        while (true) {
            cout << "user@fs:" << fullpath(cwd) << "$ ";
            if (!getline(cin, line) || line == "exit") break;
            if (line == "test") runtests();
            else execcmd(line);
        }
    }
};

int main(int argc, char** argv) {
    gameengine sim;
    string lvl = "";
    if (argc > 1) lvl = argv[1];
    cout << "Terminal Escape started. Type 'help' or 'test'.\n";
    sim.runshell(lvl);
    return 0;
}
