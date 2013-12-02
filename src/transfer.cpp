/*

MEGA SDK - Client Access Engine Core Logic

(c) 2013 by Mega Limited, Wellsford, New Zealand

Author: mo
Bugfixing: js, mr

Applications using the MEGA API must present a valid application key
and comply with the the rules set forth in the Terms of Service.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include "mega/transfer.h"
#include "mega/megaclient.h"
#include "mega/transferslot.h"
#include "mega/megaapp.h"

Transfer::Transfer(MegaClient* cclient, direction ctype)
{
	type = ctype;
	client = cclient;

	failcount = 0;
	uploadhandle = 0;
	slot = NULL;
}

// delete transfer with underlying slot, notify files
Transfer::~Transfer()
{
	for (file_list::iterator it = files.begin(); it != files.end(); it++) (*it)->transfer = NULL;
	client->transfers[type].erase(transfers_it);
	delete slot;
}

// transfer attempt failed, notify all related files, collect request on whether to abort the transfer, kill transfer if unanimous
void Transfer::failed(error e)
{
	bool defer;

	bt.backoff(client->waiter->ds);
	client->app->transfer_failed(this,e);

	for (file_list::iterator it = files.begin(); it != files.end(); it++) if ((*it)->failed(e) && !defer) defer = true;

	if (defer)
	{
		failcount++;
		delete slot;
	}
	else delete this;
}

// transfer completion: copy received file locally, set timestamp(s), verify fingerprint, notify app, notify files
void Transfer::complete()
{
	if (type == GET)
	{
		// disconnect temp file from slot...
		delete slot->file;
		slot->file = NULL;

		// set timestamp (subsequent moves & copies are assumed not to alter mtime)
		client->fsaccess->setmtimelocal(&localfilename,mtime);

		// verify integrity of file
		FileAccess* fa = client->fsaccess->newfileaccess();
		FileFingerprint fingerprint;
		Node* n;

		if (fa->fopen(&localfilename,true,false))
		{
			fingerprint.genfingerprint(fa);
			delete fa;

			if (isvalid && !(fingerprint == *(FileFingerprint*)this))
			{
				client->fsaccess->unlinklocal(&localfilename);
				return failed(API_EWRITE);
			}
		}

		// set FileFingerprint on source node(s) if missing
		for (file_list::iterator it = files.begin(); it != files.end(); it++)
		{
			if ((n = client->nodebyhandle((*it)->h)))
			{
				if (!n->isvalid)
				{
					*(FileFingerprint*)n = fingerprint;

					n->serializefingerprint(&n->attrs.map['c']);
					client->setattr(n);
				}
			}
		}

		// ...and place it in all target locations
		string* renamedto = NULL;

		// rename file to one of its final target locations
		for (file_list::iterator it = files.begin(); it != files.end(); it++)
		{
			if (client->fsaccess->renamelocal(&localfilename,&(*it)->localname))
			{
				renamedto = &(*it)->localname;
				break;
			}
		}

		// copy to the other remaining target locations
		for (file_list::iterator it = files.begin(); it != files.end(); it++)
		{
			if (renamedto != &(*it)->localname) client->fsaccess->copylocal(renamedto ? renamedto : &localfilename,&(*it)->localname);
		}

		if (!renamedto) client->fsaccess->unlinklocal(&localfilename);
	}
	else
	{
		// files must not change during a PUT transfer
		if (genfingerprint(slot->file)) return failed(API_EREAD);
	}

	client->app->transfer_complete(this);

	// notify all files and give them an opportunity to self-destruct
	for (file_list::iterator it = files.begin(); it != files.end(); )
	{
		Transfer* t = (*it)->transfer;

		(*it)->transfer = NULL;
		(*it++)->completed(t,NULL);
	}

	delete this;
}

