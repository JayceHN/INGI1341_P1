#include "packet_interface.h"
#include <zlib.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>

pkt_status_code code;

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
		pkt_t *packet = (pkt_t *)calloc(1, sizeof(pkt_t));
		return packet;
}


/* Libère le pointeur vers la struct pkt, ainsi que toutes les
 * ressources associées
 */
void pkt_del(pkt_t *pkt)
{
    free(pkt);
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
		if(pkt == NULL)
		{
			return 0;
		}
		return pkt->seqnum;
}

uint16_t pkt_get_length(const pkt_t *pkt)
{
		return pkt->length;
}

uint32_t pkt_get_timestamp   (const pkt_t *pkt)
{
		if(pkt == NULL)
		{
			return 61;
		}
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
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_window(pkt_t *pkt, const uint8_t win)
{
		pkt->window = win;
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_seqnum(pkt_t *pkt, const uint8_t seqnum)
{
		pkt->seqnum = seqnum;
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_length(pkt_t *pkt, const uint16_t length)
{
		pkt->length = length;
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_timestamp(pkt_t *pkt, const uint32_t timestamp)
{
		pkt->timestamp = timestamp;
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_crc(pkt_t *pkt, const uint32_t crc)
{
		pkt->crc = crc;
		code = PKT_OK;
		return code;
}

pkt_status_code pkt_set_payload(pkt_t *pkt, const char *data, const uint16_t length)
{
		pkt->payload = calloc(1, (sizeof(char) * length));
		if(pkt->payload == NULL)
		{
			code = E_NOMEM;
			return code;

		}
		memcpy(pkt->payload, data, length);
		pkt_set_length(pkt, length);
		code = PKT_OK;
		return code;
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
			//variables
			uint32_t header = 0;
			uint32_t timestamp = 0;
			uint32_t crc = 0;

			// decoder le header
			memcpy(&header, data, sizeof(uint32_t));

			// reconversion en host order
			header = ntohl(header);

			uint16_t length = header;
			uint8_t seqnum = header >> 16;
			uint8_t window = (header << 3) >> 27;
			uint8_t type = header >> 29;

			if(type != PTYPE_DATA && type != PTYPE_ACK)
			{
					code = E_TYPE;
					return code;
			}

			pkt_set_type(pkt, type);
			pkt_set_window(pkt, window);
			pkt_set_seqnum(pkt, seqnum);
			pkt_set_length(pkt, length);

			// decoder le timestamp
			memcpy(&timestamp, data+sizeof(uint32_t), sizeof(uint32_t));
			pkt_set_timestamp(pkt, timestamp);

			// si la longuer du payload ne correspond pas a ce qui est marque dans le paquet
			if(pkt_get_length(pkt) != len-3*sizeof(uint32_t))
			{
					code = E_LENGTH;
					return code;
			}

			//decoder le payload
			pkt_set_payload(pkt, data+(2*sizeof(uint32_t)), pkt_get_length(pkt));

			//decode le crc;
			memcpy(&crc, data+(2*sizeof(uint32_t)+pkt_get_length(pkt)), sizeof(uint32_t));

			crc = ntohl(crc);
			pkt_set_crc(pkt, crc);

			uint32_t newCrc = 0;
			newCrc = crc32(newCrc, (Bytef *)data, (2*sizeof(uint32_t)+pkt_get_length(pkt)));

			// verifie le crc
			if(newCrc != pkt_get_crc(pkt))
			{
				code =	E_CRC;
				return code;
			}

			// le paquet est valide
			code = PKT_OK;
			return code;
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
			// pas assez de place dans le buffer
			if(*len < (3*sizeof(uint32_t)+pkt_get_length(pkt)))
			{
					code = E_NOMEM;
					return code;
			}

			// mettre a 0 le contenu du buffer
			memset(buf, 0, *len);

			//variables locales
			uint32_t header = 0;
			uint32_t tmp = 0;
			size_t size = 0;

			// construction du header
			header = (header | pkt->type) << 29;		// ajout du type
			tmp = pkt->window << 24;								// ajout du window
			header = (header | tmp);
			tmp = pkt->seqnum << 16;								// ajout seqnum
			header = (header | tmp);
			header = (header | pkt->length);				//ajout length


			//conversion en network-byte-order et ajout au buffer
			header = htonl(header);
			memcpy(buf, &header, sizeof(uint32_t));
			size = size + sizeof(uint32_t);		// taille occupee dans le buffer ++

			// creation du timestamp en network-byte-order
			uint32_t timestamp = 0;
			timestamp = pkt_get_timestamp(pkt);
			memcpy(buf+size, &timestamp, sizeof(uint32_t));
			size = size+sizeof(uint32_t); 	// taille occupee dans le buffer ++

			//ajout du payload dans le buffer
			memcpy(buf+size, pkt_get_payload(pkt), pkt_get_length(pkt));
			size = size+pkt_get_length(pkt);	// taille occupee dans le buffer ++

			// calcul et ajout du crc en network-byte-order dans le buffer
			uint32_t crc = 0;
			crc = crc32(crc, (Bytef *)buf, size);
			crc = htonl(crc);

			memcpy(buf+size, &crc, sizeof(uint32_t));
			size = size+sizeof(uint32_t);		// taille occupee dans le buffer ++
			*len = size;						// taille finale occuppee dans le buffer

			code = PKT_OK;
			return code;
}
