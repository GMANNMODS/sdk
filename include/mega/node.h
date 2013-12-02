/*

MEGA SDK - Client Access Engine Core Logic

(c) 2013 by Mega Limited, Wellsford, New Zealand

Applications using the MEGA API must present a valid application key
and comply with the the rules set forth in the Terms of Service.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#ifndef MEGA_NODE_H
#define MEGA_NODE_H 1

#include "filefingerprint.h"
#include "file.h"
#include "attrmap.h"

struct NodeCore
{
	static const int FILENODEKEYLENGTH = 32;
	static const int FOLDERNODEKEYLENGTH = 16;

	// node's own handle
	handle nodehandle;

	// parent node handle (in a Node context, temporary placeholder until parent is set)
	handle parenthandle;

	// node type
	nodetype type;

	// full folder/file key, symmetrically or asymmetrically encrypted
	string nodekey;

	// new node's client-controlled timestamp (should be last modification)
	time_t clienttimestamp;

	// node attributes
	string attrstring;
};

// new node for putnodes()
struct NewNode : public NodeCore
{
	static const int UPLOADTOKENLEN = 27;

	newnodesource source;

	handle uploadhandle;
	byte uploadtoken[UPLOADTOKENLEN];

	handle syncid;
	LocalNode* localnode;

	NewNode() { syncid = UNDEF; }
};

// filesystem node
struct Node : public NodeCore, Cachable, FileFingerprint
{
	MegaClient* client;

	// node crypto keys
	string keystring;

	// change parent node association
	bool setparent(Node*);

	// copy JSON-delimited string
	static void copystring(string*, const char*);

	// try to resolve node key string
	bool applykey();

	// decrypt attribute string and set fileattrs
	void setattr();

	// display name (UTF-8)
	const char* displayname();

	// node-specific key
	SymmCipher key;

	// node attributes
	AttrMap attrs;

	// owner
	handle owner;

	// actual time this node was created (cannot be set by user)
	time_t ctime;

	// FILENODE nodes only: size, fingerprint, nonce, meta MAC, attribute string
	int64_t ctriv;
	int64_t metamac;

	string fileattrstring;

	// check presence of file attribute
	int hasfileattribute(fatype) const;

	// decrypt node attribute string
	static byte* decryptattr(SymmCipher*, const char*, int);

	// inbound share
	Share* inshare;

	// outbound shares by user
	share_map outshares;

	// incoming/outgoing share key
	SymmCipher* sharekey;

	// app-private pointer
	void* appdata;

	bool removed;

	void setkey(const byte* = NULL);

	void setfingerprint();

	void faspec(string*);

	// parent
	Node* parent;

	// children
	node_list children;

	// own position in parent's children
	node_list::iterator child_it;

	// own position in fingerprint set (only valid for file nodes)
	fingerprint_set::iterator fingerprint_it;

	// related synced item or NULL
	LocalNode* localnode;

	// active sync get
	struct SyncFileGet* syncget;

	// active removal to SyncDebris
	bool syncdeleted;

	// source tag
	int tag;

	// check if node is below this node
	bool isbelow(Node*) const;

	bool serialize(string*);
	static Node* unserialize(MegaClient*, string*, node_vector*);

	Node(MegaClient*, vector<Node*>*, handle, handle, nodetype, m_off_t, handle, const char*, time_t, time_t);
	~Node();
};

struct LocalNode : public File
{
	class Sync* sync;

	// parent linkage
	LocalNode* parent;

	// children by name
	localnode_map children;

	// related cloud node, if any
	Node* node;

	// FILENODE or FOLDERNODE
	nodetype type;

	// detection of deleted filesystem records
	handle scanseqno;

	// global sync reference
	handle syncid;

	// for folders: generic OS filesystem notification handle/pointer
	void* notifyhandle;

	// build full local path to this node
	void getlocalpath(MegaClient*, string*);

	void prepare();
	void completed(Transfer*, LocalNode*);

	void setnode(Node*);

	void init(Sync*, string*, nodetype, LocalNode*, string*);

	~LocalNode();
};

#endif
