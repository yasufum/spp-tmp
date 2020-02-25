# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
import copy


class BaseComplAction(object):
    """Base class for completion of actions list.

    Each action inherits from this class.
    Provides base functionality and command completion.
    """

    def __init__(self, data=None):
        if data is not None:
            self.data = data

    def compl_action(self, tokens, index):
        """Completion for actions list.

        Complement using the information defined in the inherited class.
        """
        candidates = []

        while index < len(tokens):
            if tokens[index - 1] == "/":
                # Completion processing end when "/" is specified
                candidates = []
                break

            elif tokens[index - 1] in self.DATA_FIELDS:
                tmp_token = self.DATA_FIELDS_VALUES.get(tokens[index - 1])
                if tmp_token is not None:
                    candidates = [tmp_token]
                else:
                    candidates = []

            else:
                # Data fields candidate and end token
                candidates = copy.deepcopy(self.DATA_FIELDS)
                candidates.append("/")

            index += 1

        return (candidates, index)


class ComplJump(BaseComplAction):
    """Complete action `jump`."""

    # Jump data fields
    DATA_FIELDS = ["group"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "group": "UNSIGNED_INT"
    }


class ComplOfPopVlan(BaseComplAction):
    """Complete action `of_pop_vlan`."""

    # of_pop_vlan data fields
    DATA_FIELDS = []

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
    }


class ComplOfPushVlan(BaseComplAction):
    """Complete action `of_push_vlan`."""

    # of_push_vlan data fields
    DATA_FIELDS = ["ethertype"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "ethertype": "UNSIGNED_INT"
    }


class ComplOfSetVlanPCP(BaseComplAction):
    """Complete action `of_set_vlan_pcp`."""

    # of_set_vlan_pcp data fields
    DATA_FIELDS = ["vlan_pcp"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "vlan_pcp": "UNSIGNED_INT"
    }


class ComplOfSetVlanVID(BaseComplAction):
    """Complete action `of_set_vlan_vid`."""

    # of_set_vlan_vid data fields
    DATA_FIELDS = ["vlan_vid"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "vlan_vid": "UNSIGNED_INT"
    }


class ComplQueue(BaseComplAction):
    """Complete action `queue`."""

    # Queue data fields
    DATA_FIELDS = ["index"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "index": "UNSIGNED_INT"
    }
