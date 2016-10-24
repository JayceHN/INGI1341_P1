#include "packet_interface.h"
#include <zlib.h>
#include <stdint.h>

/* Extra #includes */
/* Your code will be inserted here */


/*
*
*/
struct __attribute__((__packed__)) pkt {
	ptypes_t type;
	uint8_t window : 5;
	uint8_t seqnum;
	uint16_t length;
	uint32_t timestamp;
	uint32_t crc;
	char *payload;

};


/* Alloue et initialise une struct pkt
 * @return: NULL en cas d'erreur */
pkt_t* pkt_new()
{
		pkt_t packet = (pkt_t *)malloc(sizeof(pkt_t));
		return packet;
}


/* Libère le pointeur vers la struct pkt, ainsi que toutes les
 * ressources associées
 */
void pkt_del(pkt_t *pkt)
{
		free(pkt->payload);
    free(pkt);
}

/*
 * Décode des données reçues et crée une nouvelle structure pkt.
 * Le paquet reçu est en network byte-order.
 * La fonction vérifie que:
 * - Le CRC32 des données reçues est le même que celui décodé à la fin
 *   du flux de données
 * - Le type du paquet est valide
 * - La longeur du paquet est valide et cohérente avec le nombre d'octets
 *   reçus.
 *
 * @data: L'ensemble d'octets constituant le paquet reçu
 * @len: Le nombre de bytes reçus
 * @pkt: Une struct pkt valide
 * @post: pkt est la représentation du paquet reçu
 *
 * @return: Un code indiquant si l'opération a réussi ou représentant
 *         l'erreur rencontrée.
 */
pkt_status_code pkt_decode(const char *data, const size_t len, pkt_t *pkt)
{

		// verifie si le type du packet est valide
		if(pkt->type != ptypes_t.PTYPE_DATA)
		{
			return pkt_status_code.E_TYPE;
		}

		uint32_t crc = 0;
		char buffer [sizeof(uint32_t)];
		char tab [sizeof(uint32_t)+pkt->length];
		memset(buffer, 0, strlen(buffer));
		memset(tab, 0, strlen(tab);

		uint8_t header = 0;
		header = header | pkt->window;
		header = header | << 3;
		header = header | pkt->type;

		buffer[0] = header;
		buffer[1] = pkt->seqnum ;
		buffer[2] = (pkt->length >> 8);
		buffer[3] = pkt->length;

		strcpy(tab, buffer);
		strcat(tab, pkt->payload);
		crc = crc32(crc, tab, strlen(tab));

		// If it is a data package first check crc32
		if(crc != ntohl(pkt->crc))
		{
			return pkt_status_code.E_CRC;
		}


}

/*
 * Encode une struct pkt dans un buffer, prêt à être envoyé sur le réseau
 * (c-à-d en network byte-order), incluant le CRC32 du header et payload.
 *
 * @pkt: La structure à encoder
 * @buf: Le buffer dans lequel la structure sera encodée
 * @len: La taille disponible dans le buffer
 * @len-POST: Le nombre de d'octets écrit dans le buffer
 * @return: Un code indiquant si l'opération a réussi ou E_NOMEM si
 *         le buffer est trop petit.
 */
pkt_status_code pkt_encode(const pkt_t* pkt, char *buf, size_t *len)
{

}

ptypes_t pkt_get_type(const pkt_t *pkt)
{
		return pkt->type;
}

uint8_t  pkt_get_window(const pkt_t *pkt)
{
		return pkt->window;
}

uint8_t  pkt_get_seqnum(const pkt_t *pkt)
{
		return pkt->seqnum;
}

uint16_t pkt_get_length(const pkt_t *pkt)
{
		return pkt->length;
}

uint32_t pkt_get_timestamp   (const pkt_t *pkt)
{
		return pkt->timestamp;
}

uint32_t pkt_get_crc   (const pkt_t *pkt)
{
		return pkt->crc;
}

const char* pkt_get_payload(const pkt_t *pkt)
{
		return pkt->payload;
}


pkt_status_code pkt_set_type(pkt_t *pkt, const ptypes_t type)
{
		pkt->type = type;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t window)
{
		pkt->window = window;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
		pkt->seqnum = seqnum;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
		pkt->length = length;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
		pkt->timestamp = timestamp;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_crc(pkt_t *pkt, const uint32_t crc)
{
		pkt->crc = crc;
		return  pkt_status_code.PKT_OK;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
		pkt->payload = calloc(sizeof(char *)length);
		if(pkt->payload == NULL)
		{
				return pkt_status_code.E_NOMEM;
		}

		memcpy(pkt->payload, data, length);
		pkt->length = length;
		return  pkt_status_code.PKT_OK;
}

		pkt_set_length(pkt, length);

}
