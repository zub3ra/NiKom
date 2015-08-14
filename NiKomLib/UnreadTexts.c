#include "NiKomStr.h"
#include "NiKomLib.h"
#include "NiKomBase.h"

#include "UnreadTexts.h"
#include "Funcs.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* ******* Local functions ******** */

void shiftBitmap(struct UnreadTexts *unreadTexts, int textNumberToAccomodate);
void trimLowestPossibleUnreadText(struct UnreadTexts *unreadTexts, int conf,
  struct System *Servermem);

/* ******* Library functions ******** */

/****** nikom.library/ChangeUnreadTextStatus *************************

    NAME
        ChangeUnreadTextStatus -- Marks a text as read or unread.

    SYNOPSIS
        ChangeUnreadTextStatus(textNumber, markAsUnread, unreadTexts)
                               D0          D1            A0
        void ChangeUnreadTextStatus(int, int, struct UnreadTexts *);

    FUNCTION
        Marks the given text number as either read or unread in the
        given unreadTexts struct.

    INPUTS
        textNumber   - the text number to mark.
        markAsUnread - 0 to mark as read, non-zero to mark as unread.
        unreadTexts  - the UnreadTexts structure to make changes to.

**********************************************************************/

void __saveds __asm LIBChangeUnreadTextStatus(
   register __d0 int textNumber,
   register __d1 int markAsUnread,
   register __a0 struct UnreadTexts *unreadTexts,
   register __a6 struct NiKomBase *NiKomBase) {

   if(textNumber < unreadTexts->bitmapStartText) {
      return;
   }
   if(textNumber >= (unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE)) {
     if(markAsUnread) {
       return;
     }
     shiftBitmap(unreadTexts, textNumber);
   }

   if(markAsUnread) {
      BAMSET(unreadTexts->bitmap, textNumber % UNREADTEXTS_BITMAPSIZE);
   } else {
      BAMCLEAR(unreadTexts->bitmap, textNumber % UNREADTEXTS_BITMAPSIZE);
   }
}

/****** nikom.library/IsTextUnread ***********************************

    NAME
        IsTextUnread -- Checks if a text is unread or not

    SYNOPSIS
        status = IsTextUnread(textNumber, unreadTexts)
        D0                    D0          A0
        int IsTextUnread(int, struct UnreadTexts *);

    FUNCTION
        Checks if the given text number is unread or not. Texts lower
        than the range of the internal bitmap are always considered to
        be read. Texts higher than the range are always considered to
        be unread.

    RESULT
        status - non-zero if the given text is unread, zero otherwise.

    INPUTS
        textNumber   - the text number to check.
        unreadTexts  - the UnreadTexts structure to make changes to.

**********************************************************************/

int __saveds __asm LIBIsTextUnread(
  register __d0 int textNumber,
  register __a0 struct UnreadTexts *unreadTexts,
  register __a6 struct NiKomBase *NiKomBase) {

  if(textNumber < unreadTexts->bitmapStartText) {
    return 0;
  }
  if(textNumber >= (unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE)) {
    return 1;
  }
  return BAMTEST(unreadTexts->bitmap, textNumber % UNREADTEXTS_BITMAPSIZE);
}

/****** nikom.library/InitUnreadTexts ********************************

    NAME
        InitUnreadTexts -- Initializes an UnreadTexts struct.

    SYNOPSIS
        InitUnreadTexts(unreadTexts)
                        A0
        void InitUnreadTexts(struct UnreadTexts *);

    FUNCTION
        Initializes the given UnreadTexts structure to have all texts
        marked as unread.

    INPUTS
        unreadTexts  - the UnreadTexts structure to initialize.

**********************************************************************/

void __saveds __asm LIBInitUnreadTexts(
   register __a0 struct UnreadTexts *unreadTexts,
   register __a6 struct NiKomBase *NiKomBase) {
  unreadTexts->bitmapStartText = NiKomBase->Servermem->info.lowtext;
  memset(unreadTexts->bitmap, 0xff, UNREADTEXTS_BITMAPSIZE/8);
  memset(unreadTexts->lowestPossibleUnreadText, 0, MAXMOTE * sizeof(long));
}

/****** nikom.library/FindNextUnreadText *****************************

    NAME
        FindNextUnreadText -- Finds the next unread text in a conf.

    SYNOPSIS
        textNumber = FindNextUnreadText(searchStart, conf, unreadTexts)
        D0                              D0           D1    A0
        int FindNextUnreadText(int, int, struct UnreadTexts *);

    FUNCTION
        Searches for the lowest unread text in the given conference
        starting from searchStart. If the given search start is
        lower than either the lowest text in the system or the range
        of the internal bitmap the search will start from the lowest
        valid text number. I.e. if you want to find the lowest unread
        text available it's safe to use 0 as search start.

    INPUTS
        searchStart - the text number to start searching from.
        conf        - The conference to search in.
        unreadTexts - the UnreadTexts structure to search in

    RESULT
        textNumber - The lowest unread text that is higher or equal
                     to search start. Or -1 if no unread text is found.

**********************************************************************/

int __saveds __asm LIBFindNextUnreadText(
  register __d0 int searchStart,
  register __d1 int conf,
  register __a0 struct UnreadTexts *unreadTexts,
  register __a6 struct NiKomBase *NiKomBase) {

  int i;

  trimLowestPossibleUnreadText(unreadTexts, conf, NiKomBase->Servermem);
  if(searchStart < unreadTexts->lowestPossibleUnreadText[conf]) {
    searchStart = unreadTexts->lowestPossibleUnreadText[conf];
  }

  for(i = searchStart; i <= NiKomBase->Servermem->info.hightext; i++) {
    if(conf != NiKomBase->Servermem->texts[i%MAXTEXTS]) {
      continue;
    }
    if(i >= (unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE)) {
      return i;
    }
    if(BAMTEST(unreadTexts->bitmap, i % UNREADTEXTS_BITMAPSIZE)) {
      return i;
    }
  }
  return -1;
}

/****** nikom.library/CountUnreadTexts *******************************

    NAME
        CountUnreadTexts -- Counts the number of unread texts in a conf.

    SYNOPSIS
        count = CountUnreadTexts(conf, unreadTexts)
        D0                       D0    A0
        int CountUnreadTexts(int, struct UnreadTexts *);

    FUNCTION
        Counts the number of unread texts in the conference.

    INPUTS
        conf        - The conference to count.
        unreadTexts - the UnreadTexts structure to search in

    RESULT
        count - The number of unread texts in the conference.

**********************************************************************/

int __saveds __asm LIBCountUnreadTexts(
  register __d0 int conf,
  register __a0 struct UnreadTexts *unreadTexts,
  register __a6 struct NiKomBase *NiKomBase) {

  int i, cnt = 0;

  trimLowestPossibleUnreadText(unreadTexts, conf, NiKomBase->Servermem);

  for(i = unreadTexts->lowestPossibleUnreadText[conf];
      i <= NiKomBase->Servermem->info.hightext; i++) {
    if(NiKomBase->Servermem->texts[i % MAXTEXTS] == conf
       && (i >= (unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE)
           || BAMTEST(unreadTexts->bitmap, i % UNREADTEXTS_BITMAPSIZE))) {
         cnt++;
    }
  }  
  return cnt;
}

/****** nikom.library/SetUnreadTexts *********************************

    NAME
        SetUnreadTexts -- Marks the given number of texts as unread

    SYNOPSIS
        SetUnreadTexts(conf, amount, unreadTexts)
        D0             D0    D1      A0
        SetUnreadTexts(int, int, struct UnreadTexts *);

    FUNCTION
        Marks the 'amount' highest texts in the given conference as
        unread and all other texts as read.

    INPUTS
        conf        - The conference to mark unread texts in.
        amount      - The amount of texts to mark as unread.
        unreadTexts - the UnreadTexts structure to change

**********************************************************************/

void __saveds __asm LIBSetUnreadTexts(
  register __d0 int conf,
  register __d1 int amount,
  register __a0 struct UnreadTexts *unreadTexts,
  register __a6 struct NiKomBase *NiKomBase) {

  int i, lowestUnreadText = NiKomBase->Servermem->info.hightext + 1;

  for(i = NiKomBase->Servermem->info.hightext;
      (i >= unreadTexts->bitmapStartText)
        && (i >= NiKomBase->Servermem->info.lowtext);
      i--) {
    if(NiKomBase->Servermem->texts[i % MAXTEXTS] == conf) {
      if(amount) {
        ChangeUnreadTextStatus(i, 1, unreadTexts);
        amount--;
        lowestUnreadText = i;
      } else {
        ChangeUnreadTextStatus(i, 0, unreadTexts);
      }
    }
  }
  unreadTexts->lowestPossibleUnreadText[conf] = lowestUnreadText;
}

struct ConfAndText {
  long conf, text;
};

/****** nikom.library/ReadUnreadTexts ********************************

    NAME
        ReadUnreadTexts -- Reads an UnreadTexts structure from disk

    SYNOPSIS
        success = ReadUnreadTexts(unreadTexts, userId)
        D0                        A0           D0
        int ReadUnreadTexts(struct UnreadTexts *, int);

    FUNCTION
        Read the unread texts data for the given user into the given
        UnreadTexts structure.

    INPUTS
        unreadTexts - the UnreadTexts structure to read into.
        userId      - The user to read data for.

    RESULT
        succes - Non-zero on success, zero on failure.

**********************************************************************/

int __saveds __asm LIBReadUnreadTexts(
  register __a0 struct UnreadTexts *unreadTexts,
  register __d0 int userId,
  register __a6 struct NiKomBase *NiKomBase) {
  BPTR file;
  char filepath[41];
  int readRes;
  struct ConfAndText cat;

  ObtainSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);

  MakeUserFilePath(filepath, userId, "Bitmap0");
  if(!(file = Open(filepath, MODE_OLDFILE))) {
    ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
    return 0;
  }
  if(Read(file, unreadTexts->bitmap, UNREADTEXTS_BITMAPSIZE/8)
     != UNREADTEXTS_BITMAPSIZE/8) {
    Close(file);
    ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
    return 0;
  }
  unreadTexts->bitmapStartText = NiKomBase->Servermem->info.lowtext;

  memset(unreadTexts->lowestPossibleUnreadText, 0, MAXMOTE * sizeof(long));
  while((readRes = Read(file,&cat,sizeof(struct ConfAndText)))
        == sizeof(struct ConfAndText)) {
    unreadTexts->lowestPossibleUnreadText[cat.conf] = cat.text;
  }

  Close(file);
  ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
  return readRes == 0;
}

/****** nikom.library/WriteUnreadTexts ********************************

    NAME
        WriteUnreadTexts -- Writes an UnreadTexts structure to disk

    SYNOPSIS
        success = WriteUnreadTexts(unreadTexts, userId)
        D0                         A0           D0
        int WriteUnreadTexts(struct UnreadTexts *, int);

    FUNCTION
        Writes the unread texts data for the given user from the given
        UnreadTexts structure.

    INPUTS
        unreadTexts - the UnreadTexts structure to write.
        userId      - The user to write data for.

    RESULT
        succes - Non-zero on success, zero on failure.

**********************************************************************/

int __saveds __asm LIBWriteUnreadTexts(
  register __a0 struct UnreadTexts *unreadTexts,
  register __d0 int userId,
  register __a6 struct NiKomBase *NiKomBase) {
  BPTR file;
  char filepath[41];
  int writeRes;
  struct Mote *conf;
  struct ConfAndText cat;

  ObtainSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);

  MakeUserFilePath(filepath, userId, "Bitmap0");
  if(!(file = Open(filepath, MODE_NEWFILE))) {
    ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
    return 0;
  }
  if(Write(file, unreadTexts->bitmap, UNREADTEXTS_BITMAPSIZE/8) == -1) {
    Close(file);
    ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
    return 0;
  }

  ITER_EL(conf, NiKomBase->Servermem->mot_list, mot_node, struct Mote *) {
    if(conf->type != MOTE_FIDO) {
      continue;
    }
    cat.conf = conf->nummer;
    cat.text = unreadTexts->lowestPossibleUnreadText[conf->nummer];
    writeRes = Write(file, &cat, sizeof(struct ConfAndText));
    if(writeRes == -1) {
      break;
    }
  }

  Close(file);
  ReleaseSemaphore(&NiKomBase->Servermem->semaphores[NIKSEM_UNREAD]);
  return writeRes != -1;
}

void shiftBitmap(struct UnreadTexts *unreadTexts, int textNumberToAccomodate) {
  int textsToShiftBitmap, bytesToShiftBitmap, firstNewTextNumber, i;
  textsToShiftBitmap =
    textNumberToAccomodate - (unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE);
  bytesToShiftBitmap = (textsToShiftBitmap / 8) + 1;
  firstNewTextNumber = unreadTexts->bitmapStartText + UNREADTEXTS_BITMAPSIZE;
  for(i = 0; i < bytesToShiftBitmap; i++) {
    unreadTexts->bitmap[((firstNewTextNumber % UNREADTEXTS_BITMAPSIZE) / 8) + i] = 0xff;
  }
  unreadTexts->bitmapStartText += bytesToShiftBitmap * 8;
}

void trimLowestPossibleUnreadText(struct UnreadTexts *unreadTexts, int conf,
    struct System *Servermem) {
  if(unreadTexts->lowestPossibleUnreadText[conf] < unreadTexts->bitmapStartText) {
    unreadTexts->lowestPossibleUnreadText[conf] = unreadTexts->bitmapStartText;
  }
  if(unreadTexts->lowestPossibleUnreadText[conf] < Servermem->info.lowtext) {
    unreadTexts->lowestPossibleUnreadText[conf] = Servermem->info.lowtext;
  }
}