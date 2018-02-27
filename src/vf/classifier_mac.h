#ifndef _CLASSIFIER_MAC_H_
#define _CLASSIFIER_MAC_H_

/**
 * @file
 * SPP Classifier
 *
 * Classifier component provides packet forwarding function from
 * one port to one port. Classifier has table of virtual MAC address.
 * According to this table, classifier lookups L2 destination MAC address
 * and determines which port to be transferred to incoming packets.
 */

/* forward declaration */
struct spp_component_info;
struct spp_iterate_classifier_table_params;

/**
 * classifier(mac address) initialize globals.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_init(void);

/**
 * classifier(mac address) update component info.
 *
 * @param component_info
 *  The pointer to struct spp_component_info.@n
 *  The data for updating the internal data of classifier.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_update(struct spp_component_info *component_info);

/**
 * classifier(mac address) thread function.
 *
 * @param id
 *  The unique component ID.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_do(int id);

/**
 * classifier get component status.
 *
 *
 * @param lcore_id
 *  The logical core ID for classifier.
 * @param id
 *  The unique component ID.
 * @param params
 *  The pointer to struct spp_iterate_core_params.@n
 *  Detailed data of classifier status.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int
spp_classifier_get_component_status(unsigned int lcore_id, int id,
		struct spp_iterate_core_params *params);

/**
 * classifier(mac address) iterate classifier table.
 *
 * @param params
 *  Point to struct spp_iterate_classifier_table_params.@n
 *  Detailed data of classifier table.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int spp_classifier_mac_iterate_table(
		struct spp_iterate_classifier_table_params *params);

#endif /* _CLASSIFIER_MAC_H_ */
