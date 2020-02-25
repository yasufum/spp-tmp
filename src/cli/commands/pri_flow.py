# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
import re
import importlib
import copy
from . import pri_flow_compl_pattern as flow_compl_ptn
from . import pri_flow_compl_action as flow_compl_act


class SppPrimaryFlow(object):
    """Exec SPP primary flow command.

    SppPrimaryFlow class is intended to be used in SppPrimary class as a
    delegator for running 'flow' subcommand.

    """

    # All of flow commands
    FLOW_CMDS = ["validate", "create", "destroy", "list", "status"]

    # Attribute commands of flow rule
    ATTR_CMDS = ["group", "priority", "ingress", "egress", "transfer",
                 "pattern"]

    # Host, port and api version in URL are complemented by
    # SppCtlClient class and execute API
    FLOW_API_VALIDATE = "primary/flow_rules/port_id/{port_id}/validate"
    FLOW_API_CREATE = "primary/flow_rules/port_id/{port_id}"
    FLOW_API_DESTROY = "primary/flow_rules/{rule_id}/port_id/{port_id}"
    FLOW_API_ALL_DESTROY = "primary/flow_rules/port_id/{port_id}"

    # Completion class relevant to the pattern item type
    PTN_COMPL_CLASSES = {
        "eth": flow_compl_ptn.ComplEth,
        "vlan": flow_compl_ptn.ComplVlan,
    }

    # Completion class relevant to the action type
    ACT_COMPL_CLASSES = {
        "jump": flow_compl_act.ComplJump,
        "of_pop_vlan": flow_compl_act.ComplOfPopVlan,
        "of_push_vlan": flow_compl_act.ComplOfPushVlan,
        "of_set_vlan_pcp": flow_compl_act.ComplOfSetVlanPCP,
        "of_set_vlan_vid": flow_compl_act.ComplOfSetVlanVID,
        "queue": flow_compl_act.ComplQueue,
    }

    def __init__(self, spp_ctl_cli):
        self.spp_ctl_cli = spp_ctl_cli

        # Use as index of tokens list when complementing flow_rule.
        self._flow_rule_token_index = 0

    def run_flow(self, params):
        """Run `flow` command."""

        if len(params) == 0:
            print("Flow sub command is NULL")
            return None

        elif params[0] not in self.FLOW_CMDS:
            print("Invalid flow sub command: '{0}'".format(params[0]))
            return None

        if params[0] == "validate":
            self._run_flow_validate(params[1:])

        elif params[0] == "create":
            self._run_flow_create(params[1:])

        elif params[0] == "destroy":
            self._run_flow_destroy(params[1:])

        elif params[0] == "list":
            self._run_flow_list(params[1:])

        elif params[0] == "status":
            self._run_flow_status(params[1:])

    def complete_flow(self, tokens):
        """Completion for flow commands."""
        candidates = []

        if len(tokens) == 2:
            for kw in self.FLOW_CMDS:
                if kw.startswith(tokens[1]):
                    candidates.append(kw)

        else:
            if tokens[1] == "validate":
                candidates = self._compl_flow_rule(tokens[2:])

            elif tokens[1] == "create":
                candidates = self._compl_flow_rule(tokens[2:])

            elif tokens[1] == "destroy":
                candidates = self._compl_flow_destroy(tokens[2:])

            elif tokens[1] == "list":
                candidates = self._compl_flow_list(tokens[2:])

            elif tokens[1] == "status":
                candidates = self._compl_flow_status(tokens[2:])

        return candidates

    def _compl_flow_rule(self, tokens):
        """Completion for validate and create commands."""
        candidates = []

        self._flow_rule_token_index = 0

        if len(tokens) == 1:
            candidates = self._create_candidacy_phy_ports(tokens[0])
            return candidates

        # Next index of RES_UID
        self._flow_rule_token_index += 1

        # Completion of attribute part in flow rule
        candidates = self._compl_flow_rule_attribute(tokens)
        if self._flow_rule_token_index == len(tokens):
            return candidates

        # Completion of pattern part in flow rule
        candidates = self._compl_flow_rule_pattern(tokens)
        if self._flow_rule_token_index == len(tokens):
            return candidates

        # Completion of action part in flow rule
        candidates = self._compl_flow_rule_action(tokens)
        if self._flow_rule_token_index == len(tokens):
            return candidates

        return candidates

    def _compl_flow_rule_attribute(self, tokens):
        """Completion for flow rule in attribute."""
        candidates = []

        while self._flow_rule_token_index < len(tokens):
            # If "group" is specified, "GROUP_ID" is a candidate
            if tokens[self._flow_rule_token_index - 1] == "group":
                candidates = ["GROUP_ID"]

            # If "priority" is specified, "LEVEL" is a candidate
            elif tokens[self._flow_rule_token_index - 1] == "priority":
                candidates = ["LEVEL"]

            # If "pattern" is specified, exit attribute completion
            elif tokens[self._flow_rule_token_index - 1] == "pattern":
                candidates = []
                break

            else:
                candidates = self.ATTR_CMDS

            self._flow_rule_token_index += 1

        return candidates

    def _compl_flow_rule_pattern(self, tokens):
        """Completion for flow rule in pattern."""
        candidates = []

        while self._flow_rule_token_index < len(tokens):
            token = tokens[self._flow_rule_token_index - 1]

            if token in self.PTN_COMPL_CLASSES.keys():
                try:
                    item_cls = self.PTN_COMPL_CLASSES[token]
                    item_instance = item_cls()

                    candidates, index = item_instance.compl_item(
                        tokens, self._flow_rule_token_index)

                    self._flow_rule_token_index = index
                    if self._flow_rule_token_index == len(tokens):
                        break

                    if (tokens[self._flow_rule_token_index - 1] == "/"):
                        # Type candidate and end token
                        pattern_list = list(self.PTN_COMPL_CLASSES.keys())
                        candidates = copy.deepcopy(pattern_list)
                        candidates.append("end")

                except Exception as _:
                    candidates = []

            elif (token == "end"):
                candidates = []
                break

            else:
                # Type candidate and end token
                pattern_list = list(self.PTN_COMPL_CLASSES.keys())
                candidates = copy.deepcopy(pattern_list)
                candidates.append("end")

            self._flow_rule_token_index += 1

        return candidates

    def _compl_flow_rule_action(self, tokens):
        """Completion for flow rule in action."""
        candidates = []

        if (tokens[self._flow_rule_token_index - 1] == "end"):
            candidates = ["actions"]
            self._flow_rule_token_index += 1

        while self._flow_rule_token_index < len(tokens):
            token = tokens[self._flow_rule_token_index - 1]

            if token in self.ACT_COMPL_CLASSES.keys():
                try:
                    action_cls = self.ACT_COMPL_CLASSES[token]
                    action_instance = action_cls()

                    candidates, index = action_instance.compl_action(
                        tokens, self._flow_rule_token_index)

                    self._flow_rule_token_index = index
                    if self._flow_rule_token_index == len(tokens):
                        break

                    if (tokens[self._flow_rule_token_index - 1] == "/"):
                        # Type candidate and end token
                        action_list = list(self.ACT_COMPL_CLASSES.keys())
                        candidates = copy.deepcopy(action_list)
                        candidates.append("end")

                except Exception as _:
                    candidates = []

            elif (tokens[self._flow_rule_token_index - 1] == "end"):
                candidates = []
                break

            else:
                # Type candidate and end token
                action_list = list(self.ACT_COMPL_CLASSES.keys())
                candidates = copy.deepcopy(action_list)
                candidates.append("end")

            self._flow_rule_token_index += 1

        return candidates

    def _compl_flow_destroy(self, tokens):
        """Completion for destroy command."""
        candidates = []

        if len(tokens) == 1:
            candidates = self._create_candidacy_phy_ports(tokens[0])

        elif len(tokens) == 2:
            candidates.append("ALL")

            rule_ids = self._get_rule_ids(tokens[0])
            if rule_ids is not None:
                candidates.extend(rule_ids)

        return candidates

    def _compl_flow_list(self, tokens):
        """Completion for list command."""
        candidates = []

        if len(tokens) == 1:
            candidates = self._create_candidacy_phy_ports(tokens[0])

        return candidates

    def _compl_flow_status(self, tokens):
        """Completion for status command."""
        candidates = []

        if len(tokens) == 1:
            candidates = self._create_candidacy_phy_ports(tokens[0])

        elif len(tokens) == 2:
            rule_ids = self._get_rule_ids(tokens[0])

            if rule_ids is not None:
                candidates = rule_ids

        return candidates

    def _run_flow_validate(self, params):
        """Run `validate` command."""
        if len(params) == 0:
            print("RES_UID is NULL")
            return None

        port_id = self._parse_phy_res_uid_to_port_id(params[0])
        if port_id is None:
            print("RES_UID is invalid")
            return None

        if len(params) < 2:
            print("Flow command invalid argument")
            return None

        body = self._parse_flow_rule(params[1:])
        if body is None:
            print("Flow command invalid argument")
            return None

        url = self.FLOW_API_VALIDATE.format(port_id=port_id)

        response = self.spp_ctl_cli.post(url, body)
        if response is None or response.status_code != 200:
            print("Error: API execution failed for flow validate")
            return None

        try:
            res_body = response.json()
        except Exception as _:
            print("Error: API response is not json")
            return None

        message = res_body.get("message")
        if message is not None:
            print(message)
        else:
            print("Error: result message is None")

    def _run_flow_create(self, params):
        """Run `create` command."""
        if len(params) == 0:
            print("RES_UID is NULL")
            return None

        port_id = self._parse_phy_res_uid_to_port_id(params[0])
        if port_id is None:
            print("RES_UID is invalid")
            return None

        if len(params) < 2:
            print("Flow command invalid argument")
            return None

        body = self._parse_flow_rule(params[1:])
        if body is None:
            print("Flow command invalid argument")
            return None

        url = self.FLOW_API_CREATE.format(port_id=port_id)

        response = self.spp_ctl_cli.post(url, body)
        if response is None or response.status_code != 200:
            print("Error: API execution failed for flow create")
            return None

        try:
            res_body = response.json()
        except Exception as _:
            print("Error: API response is not json")
            return None

        message = res_body.get("message")
        if message is not None:
            print(message)
        else:
            print("Error: result message is None")

    def _run_flow_destroy(self, params):
        """Run `destroy` command."""
        if len(params) == 0:
            print("RES_UID is NULL")
            return None

        port_id = self._parse_phy_res_uid_to_port_id(params[0])
        if port_id is None:
            print("RES_UID is invalid")
            return None

        if len(params) < 2:
            print("RULE_ID is NULL")
            return None

        all_destroy = False

        if params[1] == "ALL":
            all_destroy = True
        else:
            try:
                rule_id = int(params[1])
            except Exception as _:
                print("RULE_ID is invalid")
                return None

        if all_destroy:
            url = self.FLOW_API_ALL_DESTROY.format(port_id=port_id)
        else:
            url = self.FLOW_API_DESTROY.format(
                port_id=port_id, rule_id=rule_id)

        response = self.spp_ctl_cli.delete(url)
        if response is None or response.status_code != 200:
            print("Error: API execution failed for flow destroy")
            return None

        try:
            res_body = response.json()
        except Exception as _:
            print("Error: API response is not json")
            return None

        message = res_body.get("message")
        if message is not None:
            print(message)
        else:
            print("Error: result message is None")

    def _run_flow_list(self, params):
        """Run `list` command."""
        if len(params) != 1:
            print("RES_UID is NULL")
            return None

        port_id = self._parse_phy_res_uid_to_port_id(params[0])
        if port_id is None:
            print("RES_UID is invalid")
            return None

        status = self._get_pri_status()
        if (status is None) or ("phy_ports" not in status):
            print("Failed to get primary status")
            return None

        target_flow_list = None
        for phy_port in status.get("phy_ports"):
            if phy_port.get("id") == port_id:
                target_flow_list = phy_port.get("flow")
                break

        if target_flow_list is None:
            print("'{0}' is invalid".format(params[0]))
            return None

        self._print_flow_list(target_flow_list)

    def _run_flow_status(self, params):
        """Run `status` command."""
        if len(params) == 0:
            print("RES_UID is NULL")
            return None

        port_id = self._parse_phy_res_uid_to_port_id(params[0])
        if port_id is None:
            print("RES_UID is invalid")
            return None

        if len(params) < 2:
            print("RULE_ID is NULL")
            return None

        try:
            rule_id = int(params[1])
        except Exception as _:
            print("RULE_ID is invalid")
            return None

        status = self._get_pri_status()
        if (status is None) or ("phy_ports" not in status):
            print("Failed to get primary status")
            return None

        target_flow_list = None
        for phy_port in status.get("phy_ports"):
            if phy_port.get("id") == port_id:
                target_flow_list = phy_port.get("flow")
                break

        if target_flow_list is None:
            print("'{0}' is invalid".format(params[0]))
            return None

        target_flow = None
        for flow in target_flow_list:
            if flow.get("rule_id") == rule_id:
                target_flow = flow

        if target_flow is None:
            print("RULE_ID:{0} does not exist.".format(params[1]))
            return None

        self._print_flow_status(target_flow)

    def _get_pri_status(self):
        """Get primary status."""
        try:
            res = self.spp_ctl_cli.get('primary/status')
            if res is None or res.status_code != 200:
                print("Error: receive error response from primary")
                return None
        except Exception as _:
            print("Error: there is an error sending the HTTP request")
            return None

        try:
            body = res.json()
        except Exception as _:
            print("Error: response body of primary status is not json.")
            return None

        return body

    def _get_candidacy_phy_ports(self):
        """Get physical_ports candidates

        Return port_id list, for example: ["0", "1"]
        If physical_ports candidates cannot be returned, return None.
        """
        pri_status = self._get_pri_status()
        if pri_status is None:
            return None

        if "phy_ports" not in pri_status:
            return None

        candidates = []

        for port in pri_status["phy_ports"]:
            if "id" not in port:
                continue

            candidates.append(str(port["id"]))

        return candidates

    def _get_rule_ids(self, res_uid):
        """Get rule_ids condidates

        Return rule_ids list, for example: ["0", "1"]
        If rule_ids candidates cannot be returned, return None.
        """
        if not res_uid.startswith("phy:"):
            return None

        try:
            port_id = int(re.sub(r"\D", "", res_uid))
        except Exception as _:
            return None

        pri_status = self._get_pri_status()
        if pri_status is None:
            return None

        if "phy_ports" not in pri_status:
            return None

        flow_list = None
        for port in pri_status["phy_ports"]:
            if port["id"] == port_id:
                flow_list = port["flow"]
                break

        if flow_list is None:
            return None

        candidates = []
        for flow in flow_list:
            candidates.append(str(flow["rule_id"]))

        return candidates

    def _create_candidacy_phy_ports(self, text):
        """Create physical_ports candidate list

        Return phy_ports candidate list, for example: ["phy:0", "phy:1"].
        If phy_ports candidates cannot be returned, return None.
        """
        port_ids = self._get_candidacy_phy_ports()
        if port_ids is None:
            return None
        return ["phy:" + p for p in port_ids]

    def _parse_phy_res_uid_to_port_id(self, res_uid):
        """Extract port_id from phy RES_UID string

        For example, extract 0 from `phy:0` and return.
        If it is not RES_UID or not phy, return None.
        """
        if type(res_uid) is not str:
            return None

        tokens = res_uid.split(":")

        if (len(tokens) != 2) or (tokens[0] != "phy"):
            return None

        try:
            port_id = int(tokens[1])
        except Exception as _:
            return None

        return port_id

    def _parse_flow_rule(self, params):
        """Parse the flow rule and convert to dict type."""
        index = 0
        max_index = len(params) - 1
        flow_rule = {"rule": {}}

        index = self._parse_flow_rule_attribute(params, index,
                                                flow_rule["rule"])
        if index is None or index >= max_index:
            return None

        index = self._parse_flow_rule_patterns(params, index,
                                               flow_rule["rule"])
        if index is None or index >= max_index:
            return None

        index = self._parse_flow_rule_actions(params, index,
                                              flow_rule["rule"])
        if index is None:
            return None

        return flow_rule

    def _parse_flow_rule_attribute(self, params, index, flow_rule):
        """Parse attribute of flow rule and convert to dict type.

        Returns the index where "pattern" is specified
        and the attribute part is completed.
        It is abnormal that index exceeds "len(params)"
        because "pattern" is not specified.
        """
        try:
            while index < len(params):
                if params[index] == "group":
                    index += 1
                    flow_rule["group"] = int(params[index])

                elif params[index] == "priority":
                    index += 1
                    flow_rule["priority"] = int(params[index])

                elif params[index] == "ingress" or params[index] == "egress":
                    flow_rule["direction"] = params[index]

                elif params[index] == "transfer":
                    flow_rule["transfer"] = True

                elif params[index] == "pattern":
                    break

                index += 1

        except Exception as _:
            return None

        return index

    def _parse_flow_rule_patterns(self, params, index, flow_rule):
        """Parse patterns of flow rule and convert to dict type.

        Returns the index where "end" is specified
        and the pattern part is completed.
        It is abnormal that index exceeds "len(params)"
        because "end" is not specified.
        """
        sentensce = ""
        flow_rule["pattern"] = []

        if params[index] != "pattern":
            return None

        index += 1

        while index < len(params):
            if params[index] == "/":
                flow_rule["pattern"].append(sentensce.rstrip())
                sentensce = ""

            elif params[index] == "end":
                index += 1
                break

            else:
                sentensce += params[index] + " "

            index += 1

        return index

    def _parse_flow_rule_actions(self, params, index, flow_rule):
        """Parse actions of flow rule and convert to dict type.

        Returns the index where "end" is specified
        and the action part is completed.
        It is abnormal that index exceeds "len(params)"
        because "end" is not specified.
        """
        sentensce = ""
        flow_rule["actions"] = []

        if params[index] != "actions":
            return None

        index += 1

        while index < len(params):
            if params[index] == "/":
                flow_rule["actions"].append(sentensce.rstrip())
                sentensce = ""

            elif params[index] == "end":
                index += 1
                break

            else:
                sentensce += params[index] + " "

            index += 1

        return index

    def _print_flow_list(self, flow_list):
        """Print flow information as a list.

        Print example.
        -----
        spp > pri; flow list phy:0
        ID      Group   Prio    Attr    Rule
        0       1       0       -e-     ETH => OF_PUSH_VLAN OF_SET_VLAN_VID
        1       1       0       i--     ETH VLAN => QUEUE OF_POP_VLAN
        2       0       0       i--     ETH => JUMP
        -----
        """
        print("ID      Group   Prio    Attr    Rule")

        for flow in flow_list:
            print_data = {}

            try:
                print_data["id"] = str(flow.get("rule_id")).ljust(7)

                attr = flow.get("attr")
                if attr is None:
                    continue

                print_data["group"] = str(attr.get("group")).ljust(7)
                print_data["prio"] = str(attr.get("priority")).ljust(7)

                ingress = "i" if attr.get("ingress") == 1 else "-"
                egress = "e" if attr.get("egress") == 1 else "-"
                transfer = "t" if attr.get("transfer") == 1 else "-"
                print_data["attr"] = "{0}{1}{2}".format(
                    ingress, egress, transfer).ljust(7)

                patterns = flow.get("patterns")
                if patterns is None:
                    continue

                print_data["rule"] = ""
                for ptn in patterns:
                    print_data["rule"] += "{0} ".format(
                        ptn.get("type").upper())
                print_data["rule"] += "=> "

                actions = flow.get("actions")
                if actions is None:
                    continue

                for act in actions:
                    print_data["rule"] += "{0} ".format(
                        act.get("type").upper())

                print("{id} {group} {prio} {attr} {rule}".format(**print_data))

            except Exception as _:
                continue

    def _print_flow_status(self, flow):
        """Print details of flow information."""
        # Attribute print
        self._print_flow_status_attribute(flow.get("attr"))

        # Patterns print
        self._print_flow_status_patterns(flow.get("patterns"))

        # Actions print
        self._print_flow_status_actions(flow.get("actions"))

    def _print_flow_status_attribute(self, attr):
        """Print attribute in the details of flow information.

        Print example.
        -----
        Attribute:
          Group   Priority Ingress Egress Transfer
          1       0        true    false  false
        -----
        """
        print_data = {}

        print_data["group"] = str(attr.get("group")).ljust(7)
        print_data["priority"] = str(attr.get("priority")).ljust(8)
        print_data["ingress"] = ("true" if attr.get("ingress") == 1
                                 else "false").ljust(7)
        print_data["egress"] = ("true" if attr.get("egress") == 1
                                else "false").ljust(6)
        print_data["transfer"] = ("true" if attr.get("transfer") == 1
                                  else "false").ljust(8)

        print("Attribute:")
        print("  Group   Priority Ingress Egress Transfer")
        print("  {group} {priority} {ingress} {egress} {transfer}".format(
            **print_data))

    def _print_flow_status_patterns(self, patterns):
        """Print patterns in the details of flow information.

        Print example.
        -----
        Patterns:
          - eth:
            - spec:
              - dst: 10:22:33:44:55:66
              - src: 00:00:00:00:00:00
              - type: 0xffff
            - last:
            - mask:
              - dst: ff:ff:ff:ff:ff:ff
              - src: 00:00:00:00:00:00
              - type: 0xffff
          - vlan:
            - spec:
              - tci: 0x0064
              - inner_type: 0x0000
            - last:
            - mask:
              - tci: 0xffff
              - inner_type: 0x0000
        -----
        """
        item_type_indent = 2
        item_fields_indent = 4

        try:
            print("Patterns:")

            for item in patterns:
                # Type print
                self._print_key_value(item.get("type"), None, item_type_indent)

                # Spec print
                self._print_key_value(
                    "spec", None, item_fields_indent)
                spec = item.get("spec")
                if spec is not None:
                    self._print_item_fields(spec)

                # Last print
                self._print_key_value(
                    "last", None, item_fields_indent)
                last = item.get("last")
                if last is not None:
                    self._print_item_fields(last)

                # Mask print
                self._print_key_value(
                    "mask", None, item_fields_indent)
                mask = item.get("mask")
                if mask is not None:
                    self._print_item_fields(mask)

        except Exception as _:
            print("Error: `patterns` structure of json received "
                  "from spp-ctl is invalid")
            return

    def _print_flow_status_actions(self, actions):
        """Print actions in the details of flow information.

        Print example.
        -----
        Actions:
          - queue:
            - index: 0
          - of_pop_vlan:
        -----
        """
        act_type_indent = 2
        act_fields_indent = 4

        try:
            print("Actions:")

            for act in actions:
                # Type print
                self._print_key_value(act.get("type"), None, act_type_indent)

                # Conf print
                conf = act.get("conf")
                if conf is not None:
                    self._print_action_conf(conf)

        except Exception as _:
            print("Error: `actions` structure of json received "
                  "from spp-ctl is invalid")
            return

    def _print_item_fields(self, fields_dic):
        """Print each field (spec, last or mask) of flow item."""
        item_elements_indent = 6

        for key, value in fields_dic.items():
            self._print_key_value(key, value, item_elements_indent)

    def _print_action_conf(self, conf):
        """Print conf of flow action."""
        act_elements_indent = 4

        for key, value in conf.items():
            self._print_key_value(key, value, act_elements_indent)

    def _print_key_value(self, key, value=None, indent_level=0):
        """Print key / value combination.

        Print in the format "- key: value".
        If value is None, print in "- key:" format
        Insert space for indent.
        """
        if value is not None:
            print_str = "- {key}: {value}".format(
                key=str(key), value=str(value))
        else:
            print_str = "- {key}:".format(key=str(key))

        print((" " * indent_level) + print_str)
