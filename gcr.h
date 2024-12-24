/* ************************************************************************** */
/** Descriptive File Name

  @Company
    dario's automation from some piciu on the net

  @File Name
    gcr.h

  @Summary
    GCR encode decode, generic and Mac

 */
/* ************************************************************************** */

#ifndef _GCR_H
#define _GCR_H

void gcr_init(void);
unsigned char gcr_finished(void);
void gcr_encode2(unsigned char raw_data);
unsigned char gcr_encodebyte(unsigned char raw_data);
unsigned char gcr_get_encoded(unsigned char *raw_data);
void gcr_decode(unsigned char gcr_data);
unsigned char gcr_decodebyte(unsigned char raw_data);
unsigned char gcr_valid(void);
unsigned char gcr_get_decoded(unsigned char *raw_data);

#endif
