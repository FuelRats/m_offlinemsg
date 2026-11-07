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
    "third/m_offlinemsg", "1.0",
    "Deliver PRIVMSGs to offline registered users via MemoServ", "Alex Sørlie",
    "unrealircd-6"};

MOD_INIT() { return MOD_SUCCESS; }

MOD_LOAD() {
  if (!CommandOverrideAdd(modinfo->handle, "PRIVMSG", 0, ovr_privmsg)) {
    config_error("m_offlinemsg: Failed to add PRIVMSG override");
    return MOD_FAILED;
  }
  return MOD_SUCCESS;
}

MOD_UNLOAD() { return MOD_SUCCESS; }

CMD_OVERRIDE_FUNC(ovr_privmsg) {
  const char *target;
  const char *message;
  Client *target_client;
  Client *memoserv;
  char memo_buf[512];
  char target_nick[NICKLEN + 1];
  MessageTag *mtags = NULL;

  /* Let the original handler process if not enough params */
  if (parc < 3) {
    CALL_NEXT_COMMAND_OVERRIDE();
    return;
  }

  target = parv[1];
  message = parv[2];

  /* Only intercept user-to-user PRIVMSGs, not channel messages */
  if (*target == '#' || *target == '&' || *target == '+') {
    CALL_NEXT_COMMAND_OVERRIDE();
    return;
  }

  /* Don't intercept messages from services or servers */
  if (!IsUser(client) || IsULine(client)) {
    CALL_NEXT_COMMAND_OVERRIDE();
    return;
  }

  /* Resolve the target, handling the "nick@server" form. Services aliases
   * (type services) address pseudoclients this way to route them across a
   * link (e.g. "NickServ@services.fuelrats.com"), and find_user()/find_client()
   * do not parse it — so without this an online service reached from a remote
   * server looks "offline" and its message gets misdirected to MemoServ. */
  {
    const char *at = strchr(target, '@');
    if (at) {
      size_t nick_len = (size_t)(at - target);

      /* A message addressed to a U-lined (services) server is never an
       * offline-user message. */
      Client *target_server = find_client(at + 1, NULL);
      if (target_server && IsServer(target_server) && IsULine(target_server)) {
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
      }

      if (nick_len == 0 || nick_len > NICKLEN) {
        CALL_NEXT_COMMAND_OVERRIDE();
        return;
      }
      memcpy(target_nick, target, nick_len);
      target_nick[nick_len] = '\0';
    } else {
      strlcpy(target_nick, target, sizeof(target_nick));
    }
  }

  /* If the target nick resolves to an online client — a real user or a
   * services pseudoclient — let normal PRIVMSG handling proceed. Only a
   * genuinely offline (registered but disconnected) nick falls through. */
  target_client = find_user(target_nick, NULL);
  if (target_client) {
    CALL_NEXT_COMMAND_OVERRIDE();
    return;
  }

  /* Target is offline — check if MemoServ is available */
  memoserv = find_client("MemoServ", NULL);
  if (!memoserv) {
    /* MemoServ not available, fall through to normal handling */
    CALL_NEXT_COMMAND_OVERRIDE();
    return;
  }

  /* Sender must be identified to send memos */
  if (!IsLoggedIn(client)) {
    sendnotice(client,
               "*** %s is currently offline. You must be identified with "
               "NickServ to leave an offline message.",
               target_nick);
    return;
  }

  /* Build the MemoServ SEND command and send it as a PRIVMSG
   * from the user to MemoServ, which will process it normally */
  snprintf(memo_buf, sizeof(memo_buf), "SEND %s %s", target_nick, message);

  new_message(client, recv_mtags, &mtags);
  sendto_one(memoserv, mtags, ":%s PRIVMSG %s :%s", client->id, memoserv->name,
             memo_buf);
  free_message_tags(mtags);

  /* Notify the sender */
  sendnotice(client,
             "*** %s is currently offline. Your message has been saved "
             "and will be delivered when they come online.",
             target_nick);

  /* Don't call the next handler — we've handled this message */
  return;
}
