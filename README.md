# m_offlinemsg

This UnrealIRCd module is a custom module for the FuelRats IRC network that provides transparent offline message delivery via MemoServ.

When a user sends a private message to a registered nick that is currently offline, the module automatically forwards the message to MemoServ as a memo. The sender receives a notice confirming their message was saved for delivery.

* Messages to offline registered nicks are automatically converted to MemoServ memos
* The sender must be identified with NickServ to leave offline messages
* Messages to channels, services, and online users are unaffected
* Subject to MemoServ's per-user memo limits

## Requirements

* UnrealIRCd 6.x
* Anope IRC Services with MemoServ enabled
