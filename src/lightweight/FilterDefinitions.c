/****************************************************************************
 *  Copyright 2003-2015 Dorian C. Arnold, Philip C. Roth, Barton P. Miller  *
 *                  Detailed MRNet usage rights in "LICENSE" file.          *
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include "mrnet_lightweight/FilterIds.h"
#include "mrnet_lightweight/Network.h"
#include "mrnet_lightweight/NetworkTopology.h"
#include "mrnet_lightweight/Packet.h"
#include "utils_lightweight.h"
#include "xplat_lightweight/vector.h"

FilterId TFILTER_NULL = 0;
FilterId SFILTER_WAITFORALL=0;
FilterId SFILTER_DONTWAIT=0;
FilterId SFILTER_TIMEOUT=0;


void tfilter_TopoUpdate(vector_t * ipackets,
                        vector_t* opackets,
                        vector_t* UNUSED(opackets_reverse),
                        void ** UNUSED(v),
                        Packet_t* UNUSED(pkt),
                        TopologyLocalInfo_t * info,
                        int upstream)
{
    Network_t * net = TopologyLocalInfo_get_Network(info);

    vector_t * itype_arr = new_empty_vector_t();
    vector_t * iprank_arr = new_empty_vector_t();
    vector_t * icrank_arr = new_empty_vector_t();
    vector_t * ichost_arr = new_empty_vector_t();
    vector_t * icport_arr = new_empty_vector_t();
    vector_t * iarray_lens = new_empty_vector_t();

    int * rtype_arr = NULL;
    uint32_t * rprank_arr = NULL;
    uint32_t * rcrank_arr = NULL;
    char ** rchost_arr = NULL;
    uint16_t * rcport_arr = NULL;
    uint32_t rarray_len = 0;

    int * type_arr = NULL;
    uint32_t * prank_arr = NULL;
    uint32_t * crank_arr = NULL;
    char ** chost_arr = NULL;
    uint16_t * cport_arr = NULL;
    uint32_t arr_len = 0;
    unsigned long arr_len_long = 0;

    char * format_string = NULL;
    
    unsigned i, z;
    uint32_t u;
    
    size_t data_size;
    size_t uhd_size;
    size_t ud_size;
    size_t charptr_size;

    uint32_t int32_pos;
    uint32_t uint32_pos;
    uint32_t uint16_pos;
    uint32_t char_pos;

    Packet_t * cur_packet = NULL;

    NetworkTopology_t * nt = NULL;

    Packet_t * new_packet = NULL;

    mrn_dbg_func_begin();

    mrn_dbg(5, mrn_printf(FLF, stderr, "Start of topology filter update ...\n"));
    
    // Process each input packet
    // Give theformat string and unpack the input array into 5 parallel arrays:
    // type
    // parent rank
    // child rank
    // child host
    // child rank
    
    mrn_dbg(5, mrn_printf(FLF, stderr, "ipackets have size %d\n", ipackets->size));

    for (i = 0; i < ipackets->size; i++) {
        cur_packet = (Packet_t*)(ipackets->vec[i]);

        // Get the format string
        format_string = Packet_get_FormatString(cur_packet);

        // Give the format string and extract the array
        if (Packet_unpack(cur_packet, format_string, &type_arr, &arr_len,
                          &prank_arr, &arr_len, &crank_arr,
                          &arr_len, &chost_arr, &arr_len, 
                          &cport_arr, &arr_len) == -1) {
            mrn_dbg(1, mrn_printf(FLF, stderr, 
                                  "ERROR: tfilter_TopoUpdate() - unpack(%s) failure\n",
                                  Packet_get_FormatString(cur_packet)));
        } else {
            // Putting the array pointers and its length in a vector
            pushBackElement(itype_arr, type_arr);
            pushBackElement(iprank_arr, prank_arr);
            pushBackElement(icrank_arr, crank_arr);
            pushBackElement(ichost_arr, chost_arr);
            pushBackElement(icport_arr, cport_arr);
            arr_len_long = (unsigned long) arr_len;
            pushBackElement(iarray_lens, (void*)arr_len_long);
            rarray_len += arr_len;
        }

        for (z = 0; z < arr_len; z++) {
            mrn_dbg(5, mrn_printf(FLF, stderr, "Packet contents: "
                        "type:%d prank:%u crank:%u chost:%s cport:%hu arrlen:%u \n",
                                  type_arr[z], prank_arr[z], crank_arr[z], 
                                  chost_arr[z], cport_arr[z], arr_len));
        }
    }

    // Dynamic allocation for result arrays
    data_size = sizeof(int32_t);
    uhd_size = sizeof(uint16_t);
    ud_size = sizeof(uint32_t);
    charptr_size = sizeof(char*);

    rtype_arr = (int *) malloc(rarray_len * data_size);
    rprank_arr = (uint32_t *) malloc(rarray_len * ud_size);
    rcrank_arr = (uint32_t *) malloc(rarray_len * ud_size);
    rchost_arr = (char **) malloc(rarray_len * charptr_size);
    rcport_arr = (uint16_t *) malloc(rarray_len * uhd_size);

    // Aggregating input packets to one large single result array for type, crank, prank
    //
    // Go through the vector of int pointers and put them in a single large result array
    int32_pos = 0;
    uint32_pos = 0;
    uint16_pos = 0;
    char_pos = 0;

    for (i = 0; i < itype_arr->size; i++) {
        mrn_dbg(5, mrn_printf(FLF, stderr, "size of itype_arr %"PRIsszt"\n", itype_arr->size));
	arr_len_long = (unsigned long)(iarray_lens->vec[i]);
        memcpy(rtype_arr + int32_pos,
               (int *)itype_arr->vec[i],
               (size_t)arr_len_long * data_size);

        mrn_dbg(5, mrn_printf(FLF, stderr, 
                              "copying for itype_arr: %d to rtype_arr %d\n",
                              *(int*)(itype_arr->vec[i]), *rtype_arr));

        u = arr_len;
        memcpy(rprank_arr + uint32_pos,
               iprank_arr->vec[i],
               (size_t) (u * ud_size));
        memcpy(rcrank_arr + uint32_pos,
               icrank_arr->vec[i],
               (size_t) (u * ud_size));
        memcpy(rchost_arr + char_pos,
               ichost_arr->vec[i],
               (size_t) (u * charptr_size));
        memcpy(rcport_arr + uint16_pos,
               icport_arr->vec[i],
               (size_t) (u * uhd_size));

        uint32_pos += u;
        uint16_pos += u;
        int32_pos += u;
        char_pos += u;
    }

    nt = Network_get_NetworkTopology(net);

    // Go through the parallel array to make updates to NetworkTopology
    for (i = 0; i < arr_len; i++) {
        switch(rtype_arr[i]) {
            case TOPO_NEW_BE:
                NetworkTopology_add_BackEnd(nt, rprank_arr[i], rcrank_arr[i],
                                            rchost_arr[i], rcport_arr[i]);
                break;
            case TOPO_REMOVE_RANK:
                NetworkTopology_remove_Node(nt, rcrank_arr[i]);
                break;
            case TOPO_CHANGE_PARENT:
                NetworkTopology_set_Parent(nt, rcrank_arr[i], rprank_arr[i], 0);
                break;
            case TOPO_CHANGE_PORT:
                NetworkTopology_change_Port(nt, rcrank_arr[i], rcport_arr[i]);
                break;
            case TOPO_NEW_CP:
                NetworkTopology_add_InternalNode(nt, rprank_arr[i], rcrank_arr[i],
                                                 rchost_arr[i], rcport_arr[i]);
                break;
            default:
                // TODO: report invalid update type and exit
                break;
        }
    }

    if (upstream) {
        // Create output packet
        new_packet = new_Packet_t_2(Packet_get_StreamId(ipackets->vec[0]),
                                    Packet_get_Tag(ipackets->vec[0]),
                                    format_string,
                                    rtype_arr, rarray_len, 
                                    rprank_arr, rarray_len, 
                                    rcrank_arr, rarray_len,
                                    rchost_arr, rarray_len, 
                                    rcport_arr, rarray_len);

        // tell MRNet to free result array
        Packet_set_DestroyData(new_packet, true);

        // Put the newly created packet in the output packet list
        pushBackElement(opackets, new_packet);
    }

    mrn_dbg(5, mrn_printf(FLF, stderr, "End of topo filter update ...\n"));
    mrn_dbg_func_end(); 
}

