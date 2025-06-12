#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv6-address.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("6LoWPANExample");

int main (int argc, char *argv[])
{
  bool verbose = false;
  bool tracing = false;
  uint32_t numberOfDevices = 5;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application containers to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("numberOfDevices", "Number of IoT devices", numberOfDevices);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer devices;
  devices.Create (numberOfDevices);

  NodeContainer server;
  server.Create (1);

  NodeContainer allNodes;
  allNodes.Add (devices);
  allNodes.Add (server);

  LrWpanHelper lrWpanHelper;
  NetDeviceContainer deviceInterfaces = lrWpanHelper.Install (devices);
  NetDeviceContainer serverInterface = lrWpanHelper.Install (server);

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDeviceInterfaces = sixLowPanHelper.Install (deviceInterfaces);
  NetDeviceContainer sixLowPanServerInterface = sixLowPanHelper.Install (serverInterface);

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:db8::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer deviceAddresses = ipv6.Assign (sixLowPanDeviceInterfaces);
  Ipv6InterfaceContainer serverAddresses = ipv6.Assign (sixLowPanServerInterface);

  UdpServerHelper serverHelper (9);
  ApplicationContainer serverApp = serverHelper.Install (server);
  serverApp.Start (Seconds (1.0));
  serverApp.Stop (Seconds (10.0));

  UdpClientHelper clientHelper (serverAddresses.GetAddress (0), 9);
  clientHelper.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  clientHelper.SetAttribute ("MaxPackets", UintegerValue (10));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numberOfDevices; ++i)
    {
      clientApps.Add (clientHelper.Install (devices.Get (i)));
    }

  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));


  if (tracing)
    {
      // Enable tracing for pcap
      lrWpanHelper.EnablePcapAll ("6LoWPANExample");
    }

  Simulator::Stop (Seconds (11.0));

  AnimationInterface anim ("6LoWPANExample.xml");
  anim.SetConstantPosition (server.Get (0), 10, 10);
  for (uint32_t i = 0; i < numberOfDevices; ++i)
    {
        anim.SetConstantPosition (devices.Get (i), 20 + i*5 , 20);
    }

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}