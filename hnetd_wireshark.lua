-- -*-lua-*-
--
-- $Id: hnetd_wireshark.lua $
--
-- Author: Markus Stenberg <mstenber@cisco.com>
--
-- Copyright (c) 2013 cisco Systems, Inc.
--
-- Created:       Tue Dec  3 11:13:05 2013 mstenber
-- Last modified: Tue Feb 10 19:44:44 2015 mstenber
-- Edit time:     117 min
--

-- This is Lua module which provides VERY basic dissector for TLVs we
-- transmit.

-- Usage: wireshark -X lua_script:hnetd_wireshark.lua

p_hncp = Proto("hncp", "Homenet Control Protocol")

local f_id = ProtoField.uint16('hncp.id', 'TLV id')
local f_len = ProtoField.uint16('hncp.len', 'TLV len')
local f_data = ProtoField.bytes('hncp.data', 'TLV data', base.HEX)

local f_nid_hash = ProtoField.bytes('hncp.node_identifier_hash', 
                                    'Node identifier', base.HEX)
local f_data_hash = ProtoField.bytes('hncp.data_hash',
                                     'Node data hash', base.HEX)
local f_network_hash = ProtoField.bytes('hncp.network_hash',
                                        'Network state hash', base.HEX)

local f_lid = ProtoField.uint32('hncp.llid', 'Local link identifier')
local f_rlid = ProtoField.uint32('hncp.rlid', 'Remote link identifier')
local f_upd = ProtoField.uint32('hncp.update_number', 'Update number')
local f_ms = ProtoField.uint32('hncp.ms_since_origination', 
                               'Time since origination (ms)')
local f_interval_ms = ProtoField.uint32('hncp.keepalive_interval',
                               'Keep-alive interval (ms)')

p_hncp.fields = {f_id, f_len, f_data,
                 f_nid_hash, f_data_hash, f_network_hash,
                 f_lid, f_rlid, f_upd, f_ms}

local tlvs = {
   -- dncp content
   [1]={name='link-id', 
        contents={{4, f_nid_hash},
                  {4, f_lid}},
   },
   [2]={name='req-net-hash'},
   [3]={name='req-node-data',
        contents={{4, f_nid_hash}},
   },

   [10]={name='network-hash',
        contents={{8, f_network_hash}}},

   [11]={name='node-state',
        contents={{4, f_nid_hash},
                  {4, f_upd},
                  {4, f_ms},
                  {8, f_data_hash},
        }
   },
   [12]={name='node-data', 
        contents={{4, f_nid_hash},
                  {4, f_upd}},
        recurse=true},
   [13]={name='node-data-neighbor', contents={{4, f_nid_hash},
                                              {4, f_rlid},
                                              {4, f_lid},
                                             },
   },
   [14]={name='keepalive-interval', contents={{4, f_lid},
                                              {4, f_interval}},
   },
   [15]={name='custom'},

   -- hncp content
   [33]={name='external-connection', contents={}, recurse=true},
   [34]={name='delegated-prefix'},
   [35]={name='assigned-prefix'},
   [36]={name='router-address'},
   [37]={name='dhcp-options'},
   [38]={name='dhcpv6-options'},

   [39]={name='dns-delegated-zone'},
   [40]={name='dns-domain-name'},
   [41]={name='dns-router-name'},
   [199]={name='routing-protocol'},
}


function p_hncp.dissector(buffer, pinfo, tree)
   pinfo.cols.protocol = 'hncp'

   local rec_decode
   local function data_decode(ofs, left, tlv, tree)
      for i, v in ipairs(tlv.contents)
      do
         local elen, ef = unpack(v)
         if elen > left
         then
            tree:append_text(string.format(' (!!! missing data - %d > %d (%s))',
                                           elen, left, v))
            return
         end
         tree:add(ef, buffer(ofs, elen))
         left = left - elen
         ofs = ofs + elen
      end
      if tlv.recurse 
      then
         rec_decode(ofs, left, tree)
      end
   end

   rec_decode = function (ofs, left, tree)
      if left < 4
      then
         return
      end
      local partial
      local rid = buffer(ofs, 2)
      local rlen = buffer(ofs+2, 2)
      local id = rid:uint()
      local len = rlen:uint() 
      local bs = ''
      local ps = ''
      local tlv = tlvs[id] or {}
      if tlv.name
      then
         bs = ' (' .. tlv.name .. ')'
      end
      if (len + 4) > left
      then
         len = left - 4
         ps = ' (partial)'
         partial = true
      end
      local tree2 = tree:add(buffer(ofs, len + 4), 
                             string.format('TLV %d%s - %d value bytes%s',
                                           id, bs, len, ps))
      if partial
      then
         return
      end
      local fid = tree2:add(f_id, rid)
      fid:append_text(bs)
      local flen = tree2:add(f_len, rlen)
      if len > 0
      then
         local fdata = tree2:add(f_data, buffer(ofs + 4, len))
         if tlv.contents
         then
            -- skip the tlv header (that's why +- 4)
            data_decode(ofs + 4, len, tlv, fdata)
         end
      end
      -- recursively decode the rest too, hrr :)

      -- (note that we have to round it up to next /4 boundary; stupid
      -- alignment..)
      len = math.floor((len + 3)/4) * 4

      rec_decode(ofs + len + 4, left - len - 4, tree)
   end
   rec_decode(0, buffer:len(), tree:add(p_hncp, buffer()))
end

-- register as udp dissector
udp_table = DissectorTable.get("udp.port")
udp_table:add(8808, p_hncp)
udp_table:add(38808, p_hncp)
