/*
 * m_offlinemsg - Offline Message Delivery for UnrealIRCd 6
 *
 * When a user sends a PRIVMSG to a registered nick that is offline,
 * this module intercepts the message and forwards it to MemoServ
 * as a memo on behalf of the sender.
 *
 * The sender receives a notice confirming the memo was sent.
 *
 * Copyright 2026 The Fuel Rats Mischief
 * Author: Alex Sørlie
 */

#include "unrealircd.h"

/* Forward declarations */
CMD_OVERRIDE_FUNC(ovr_privmsg);

ModuleHeader MOD_HEADER = {
    "third/m_offlinemsg",
    "1.0",
    "Deliver PRIVMSGs to offline registered users via MemoServ",
    "Alex Sørlie",
    "unrealircd-6"
};

MOD_INIT()
{
    return MOD_SUCCESS;
}

MOD_LOAD()
{
    if (!CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, ovr_privmsg))
    {
        config_error("m_offlinemsg: Failed to add PRIVMSG override");
        return MOD_FAILED;
    }
    return MOD_SUCCESS;
}

MOD_UNLOAD()
{
    return MOD_SUCCESS;
}

CMD_OVERRIDE_FUNC(ovr_privmsg)
{
    const char *target;
    const char *message;
    Client *target_client;
    Client *memoserv;
    char memo_buf[512];
    MessageTag *mtags = NULL;

    /* Let the original handler process if not enough params */
    if (parc < 3)
    {
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    target = parv[1];
    message = parv[2];

    /* Only intercept user-to-user PRIVMSGs, not channel messages */
    if (*target == '#' || *target == '&' || *target == '+')
    {
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    /* Don't intercept messages from services or servers */
    if (!IsUser(client) || IsULine(client))
    {
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    /* Don't intercept messages TO services */
    {
        Client *check = find_client(target, NULL);
        if (check && IsULine(check))
        {
            CALL_NEXT_COMMAND_OVERRIDE();
            return;
        }
    }

    /* Check if the target user is online */
    target_client = find_user(target, NULL);
    if (target_client)
    {
        /* Target is online, let normal PRIVMSG handling proceed */
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    /* Target is offline — check if MemoServ is available */
    memoserv = find_client("MemoServ", NULL);
    if (!memoserv)
    {
        /* MemoServ not available, fall through to normal handling */
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
    }

    /* Sender must be identified to send memos */
    if (!IsLoggedIn(client))
    {
        sendnotice(client,
            "*** %s is currently offline. You must be identified with "
            "NickServ to leave an offline message.",
            target);
        return;
    }

    /* Build the MemoServ SEND command and send it as a PRIVMSG
     * from the user to MemoServ, which will process it normally */
    snprintf(memo_buf, sizeof(memo_buf), "SEND %s %s", target, message);

    new_message(client, recv_mtags, &mtags);
    sendto_one(memoserv, mtags, ":%s PRIVMSG %s :%s",
               client->id, memoserv->name, memo_buf);
    free_message_tags(mtags);

    /* Notify the sender */
    sendnotice(client,
        "*** %s is currently offline. Your message has been saved "
        "and will be delivered when they come online.",
        target);

    /* Don't call the next handler — we've handled this message */
    return;
}
