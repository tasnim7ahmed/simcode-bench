#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/dhcp-client-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Configuration parameters
  uint32_t nVlan = 3;
  uint32_t hostsPerVlan = 2;
  std::vector<std::string> vlanNetworks = { "10.1.1.0", "10.1.2.0", "10.1.3.0" };
  std::vector<std::string> vlanMasks = { "255.255.255.0", "255.255.255.0", "255.255.255.0" };

  // Create nodes
  NodeContainer routerNode;
  routerNode.Create (1);
  NodeContainer switchNode;
  switchNode.Create (1);

  std::vector<NodeContainer> vlanHosts (nVlan);
  for (uint32_t i = 0; i < nVlan; ++i)
    vlanHosts[i].Create (hostsPerVlan);

  // CSMA Helpers for VLAN links (simulate trunking)
  std::vector<CsmaHelper> vlanCsma (nVlan);
  std::vector<NetDeviceContainer> hostDevices (nVlan);
  std::vector<NetDeviceContainer> switchPorts (nVlan);
  std::vector<NetDeviceContainer> routerPorts (nVlan);

  // Assign VLAN IDs starting from 10
  uint16_t baseVlanId = 10;
  std::vector<uint16_t> vlanIds (nVlan);
  for (uint32_t i = 0; i < nVlan; ++i)
    vlanIds[i] = baseVlanId + i;

  // Create VLAN subnets and configure CSMA segments per VLAN
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      // Each VLAN: hosts + switch + router
      NodeContainer vlanNodes;
      vlanNodes.Add (vlanHosts[i]);
      vlanNodes.Add (switchNode.Get (0));
      vlanNodes.Add (routerNode.Get (0));

      vlanCsma[i].SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
      vlanCsma[i].SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

      NetDeviceContainer devices = vlanCsma[i].Install (vlanNodes);

      // Index: [0, hostsPerVlan-1] - hosts; [hostsPerVlan] - switch; [hostsPerVlan+1] - router
      NetDeviceContainer hostDev;
      for (uint32_t j = 0; j < hostsPerVlan; ++j)
        hostDev.Add (devices.Get (j));
      hostDevices[i] = hostDev;
      switchPorts[i].Add (devices.Get (hostsPerVlan));
      routerPorts[i].Add (devices.Get (hostsPerVlan+1));
    }

  // Configure switch as 802.1Q (VLAN) Layer 3 switch using BridgeNetDevice
  // In NS3, BridgeNetDevice is L2; for L3 switching, assign IPs per port, simulate virtual interfaces
  // Attach bridge to switch node
  NetDeviceContainer allSwitchPorts;
  for (uint32_t i = 0; i < nVlan; ++i)
    allSwitchPorts.Add (switchPorts[i].Get (0));
  Ptr<Node> switchPtr = switchNode.Get (0);
  BridgeHelper bridge;
  bridge.Install (switchPtr, allSwitchPorts);

  // Install InternetStackHelper on all nodes
  InternetStackHelper internet;
  for (uint32_t i = 0; i < nVlan; ++i)
    internet.Install (vlanHosts[i]);
  internet.Install (routerNode);

  // Configure DHCP Server on router for each VLAN
  std::vector<Ptr<Node>> dhcpServers;
  for (uint32_t i = 0; i < nVlan; ++i)
    dhcpServers.push_back (routerNode.Get (0));

  // Assign router interface IPs (default gateway)
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> routerIfs (nVlan);
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      address.SetBase (vlanNetworks[i].c_str (), vlanMasks[i].c_str ());
      // Assign IP to the router side
      NodeContainer routerContainer;
      routerContainer.Add (routerNode.Get (0));
      routerIfs[i] = address.Assign (routerPorts[i]);
      address.NewNetwork ();
    }

  // DHCP Server setup
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      DhcpServerHelper dhcpServer;
      Ipv4Address network (vlanNetworks[i].c_str ());
      Ipv4Mask netmask (vlanMasks[i].c_str ());
      // Gateway: router interface
      Ipv4Address gateway = routerIfs[i].GetAddress (0);

      dhcpServer.SetAttribute ("Network", Ipv4AddressValue (network));
      dhcpServer.SetAttribute ("NetworkMask", Ipv4MaskValue (netmask));
      dhcpServer.SetAttribute ("PoolStart", Ipv4AddressValue (Ipv4Address (network.Get () + 10)));
      dhcpServer.SetAttribute ("PoolEnd", Ipv4AddressValue (Ipv4Address (network.Get () + 250)));
      dhcpServer.SetAttribute ("Router", Ipv4AddressValue (gateway));
      ApplicationContainer serverApps = dhcpServer.Install (routerNode.Get (0), routerPorts[i].Get (0));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (30.0));
    }

  // DHCP Client setup on each host
  for (uint32_t i = 0; i < nVlan; ++i)
    {
      for (uint32_t j = 0; j < hostsPerVlan; ++j)
        {
          DhcpClientHelper dhcpClient;
          ApplicationContainer clientApps = dhcpClient.Install (vlanHosts[i].Get (j), hostDevices[i].Get (j));
          clientApps.Start (Seconds (0.5));
          clientApps.Stop (Seconds (30.0));
        }
    }

  // Enable PCAP tracing on router (to capture DHCP request/offer)
  for (uint32_t i = 0; i < nVlan; ++i)
    vlanCsma[i].EnablePcap ("vlan-dhcp-router-vlan"+std::to_string(i+1), routerPorts[i], true, true);

  // Wait for DHCP to finish, then generate inter-VLAN UDP traffic
  uint16_t port = 4000;
  double appStart = 3.0;
  double appStop = 29.0;
  ApplicationContainer clientApps, serverApps;

  for (uint32_t srcVlan = 0; srcVlan < nVlan; ++srcVlan)
    {
      for (uint32_t dstVlan = 0; dstVlan < nVlan; ++dstVlan)
        if (srcVlan != dstVlan)
          {
            // Install packet sink on host #0 in dstVlan
            Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
            PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkLocalAddress);
            serverApps.Add (sinkHelper.Install (vlanHosts[dstVlan].Get (0)));

            // UDPClient on host #0 in srcVlan sends to host #0 in dstVlan
            OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
            clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
            clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
            clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1Mbps")));
            clientHelper.SetAttribute ("PacketSize", UintegerValue (512));

            // Destination: to be filled at runtime (assigned by DHCP)
            // Will bind later, once IP addresses assigned
            Ptr<Node> dstNode = vlanHosts[dstVlan].Get (0);
            Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4> ();
            Ipv4Address dstIp = ipv4->GetAddress (1,0).GetLocal (); // interface 1 is CSMA, 0 is loopback

            ApplicationContainer c = clientHelper.Install (vlanHosts[srcVlan].Get (0));
            c.Start (Seconds (appStart));
            c.Stop (Seconds (appStop));
            Ptr<Application> app = c.Get (0);
            // Assign destination later, in Schedule
            Simulator::Schedule (Seconds (appStart - 0.5), [app, dstIp, port] {
              Ptr<OnOffApplication> ona = DynamicCast<OnOffApplication> (app);
              if (ona)
                ona->SetAttribute ("Remote", AddressValue (InetSocketAddress (dstIp, port)));
            });
            clientApps.Add (c);
          }
    }

  // Enable global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  serverApps.Start (Seconds (appStart-0.2));
  serverApps.Stop (Seconds (appStop+0.2));

  Simulator::Stop (Seconds (30.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}