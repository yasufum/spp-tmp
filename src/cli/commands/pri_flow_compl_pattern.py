# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
import copy


class BaseComplPatternItem(object):
    """Base class for completion of pattern items.

    Each pattern item inherits from this class.
    Provides base functionality and command completion.
    """

    # Token after DATA_FIELDS
    MATCHING_PATTERN = ["is", "spec", "last", "mask", "prefix"]

    def __init__(self, data=None):
        if data is not None:
            self.data = data

    def compl_item(self, tokens, index):
        """Completion for pattern item commands.

        Complement using the information defined in the inherited class.
        """
        candidates = []

        while index < len(tokens):
            if tokens[index - 1] == "/":
                # Completion processing end when "/" is specified
                candidates = []
                break

            elif tokens[index - 1] in self.DATA_FIELDS:
                candidates = self.MATCHING_PATTERN

            elif tokens[index - 1] in self.MATCHING_PATTERN:
                if tokens[index - 1] == "prefix":
                    candidates = ["Prefix"]

                else:
                    tmp_token = self.DATA_FIELDS_VALUES.get(tokens[index - 2])
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


class ComplEth(BaseComplPatternItem):
    """Complete pattern item `eth`."""

    # Eth data fields
    DATA_FIELDS = ["dst", "src", "type"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "dst": "MAC_ADDRESS",
        "src": "MAC_ADDRESS",
        "type": "UNSIGNED_INT"
    }


class ComplVlan(BaseComplPatternItem):
    """Complete pattern item `vlan`."""

    # Vlan data fields
    DATA_FIELDS = ["tci", "pcp", "dei", "vid", "inner_type"]

    # DATA_FIELDS value candidates
    DATA_FIELDS_VALUES = {
        "tci": "UNSIGNED_INT",
        "pcp": "UNSIGNED_INT",
        "dei": "UNSIGNED_INT",
        "vid": "UNSIGNED_INT",
        "inner_type": "UNSIGNED_INT"
    }
