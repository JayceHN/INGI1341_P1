#include "packet_interface.h"

// status code that is returned
pkt_status_code code;

/*
*
*/
struct __attribute__((__packed__)) pkt {
	ptypes_t type; // PTYPE_DATA = 1 OR PTYPE_ACK = 2
	uint8_t window : 5; // BETWEEN [0, 31]
	uint8_t seqnum; // BETWEEN [0, 255]
	uint16_t length; // BETWEEN [0, 512]
	uint32_t timestamp; // OWN IMPLEMENTATION
	uint32_t crc; // RESULT OF CRC32()
	char *payload; // DATA TO BE SEND

};


pkt_t* pkt_new()
{
		pkt_t *packet = (pkt_t *)calloc(1, sizeof(pkt_t));
		return packet;
}

void pkt_del(pkt_t *pkt)
{
    free(pkt->payload);
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

			// First the header is decoded containing the TYPE, WINDOW, SEQNUM AND LENGTH
			memcpy(&header, data, sizeof(uint32_t));

			// we converte the header into host order
			header = ntohl(header);

      // we extract the different fields with bitshifts
			uint16_t length = header;
			uint8_t seqnum = header >> 16;
			uint8_t window = (header << 3) >> 27;
			uint8_t type = header >> 29;

      // if the package is not containing a valid type return error code
			if(type != PTYPE_DATA && type != PTYPE_ACK)
			{
					code = E_TYPE;
					return code;
			}

      // set the fields of the structure in host-byte-order
			pkt_set_type(pkt, type);
			pkt_set_window(pkt, window);
			pkt_set_seqnum(pkt, seqnum);
			pkt_set_length(pkt, length);

			// We continue by decoding the timestamp which is NOT converted into host-byte-order
			memcpy(&timestamp, data+sizeof(uint32_t), sizeof(uint32_t));
			pkt_set_timestamp(pkt, timestamp);

			// We check if the lenght is valid
			if(pkt_get_length(pkt) != len-3*sizeof(uint32_t))
			{
					code = E_LENGTH;
					return code;
			}

			// After checking its length we decode the payload
			pkt_set_payload(pkt, data+(2*sizeof(uint32_t)), pkt_get_length(pkt));

			// We then decode the crc
			memcpy(&crc, data+(2*sizeof(uint32_t)+pkt_get_length(pkt)), sizeof(uint32_t));

      // converting and setting crc into host-byte-order
			crc = ntohl(crc);
			pkt_set_crc(pkt, crc);

      // recomputing the crc
			uint32_t newCrc = 0;
			newCrc = crc32(newCrc, (Bytef *)data, (2*sizeof(uint32_t)+pkt_get_length(pkt)));

		  // check if the decoded crc was valid
			if(newCrc != pkt_get_crc(pkt))
			{
				code =	E_CRC;
				return code;
			}

			// The package vas valid :)
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
			// We check if there is enough place in the buffer
			if(*len < (3*sizeof(uint32_t)+pkt_get_length(pkt)))
			{
					code = E_NOMEM;
					return code;
			}

			// We initialize the buffers content to 0 before we extract data
			memset(buf, 0, *len);

			//variables
			uint32_t header = 0;
			uint32_t tmp = 0;
			size_t size = 0;

			// Here we add different fields to our header
			header = (header | pkt->type) << 29;		// ajout du type
			tmp = pkt->window << 24;								// ajout du window
			header = (header | tmp);
			tmp = pkt->seqnum << 16;								// ajout seqnum
			header = (header | tmp);
			header = (header | pkt->length);				//ajout length


			//We convert our header into network-byte-order and copy it into the buffer
			header = htonl(header);
			memcpy(buf, &header, sizeof(uint32_t));
			size = size + sizeof(uint32_t);		// we keep track of the size of the buffer

			// We copy the timestamp into the buffer
			uint32_t timestamp = 0;
			timestamp = pkt_get_timestamp(pkt);
			memcpy(buf+size, &timestamp, sizeof(uint32_t));
			size = size+sizeof(uint32_t); 	// size of buffer is  increased

			// We add the payload to the buffer
			memcpy(buf+size, pkt_get_payload(pkt), pkt_get_length(pkt));
			size = size+pkt_get_length(pkt);	// size of buffer is  increased

			// We compute the src and add it to the buffer in network-byte-order
			uint32_t crc = 0;
			crc = crc32(crc, (Bytef *)buf, size);
			crc = htonl(crc);

			memcpy(buf+size, &crc, sizeof(uint32_t));
			size = size+sizeof(uint32_t);		// size of buffer is increased
			*len = size;						       // final size of the buffer

      		// Buffer is ready to be send :)
			code = PKT_OK;
			return code;
}
