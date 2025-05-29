#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/object-names.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ObjectNamesCsmaExample");

int
main (int argc, char *argv[])
{
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  // Command line args
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create 4 nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Assign names to nodes for clarity
  Names::Add ("client", nodes.Get (0));
  Names::Add ("server", nodes.Get (1));
  Names::Add ("peer2", nodes.Get (2));
  Names::Add ("peer3", nodes.Get (3));

  // Create CSMA channel and set per-device attributes
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (0.2)));
  // We'll specifically configure 0:0 as 10Mbps for demonstration
  csma.SetDeviceAttribute ("Mtu", UintegerValue (1500));

  // Install devices; get the NetDeviceContainer
  NetDeviceContainer devices = csma.Install (nodes);

  // Assign names to devices
  for (uint32_t i = 0; i < devices.GetN (); ++i)
    {
      std::ostringstream oss;
      oss << "csma-dev" << i;
      Names::Add (oss.str (), devices.Get (i));
    }

  // Demonstrate device/device-attribute config through names
  // Let's configure the DataRate of client device via names
  Config::Set ("/Names/clientDevice/DataRate", StringValue ("10Mbps"));
  // But we've named them as csma-dev*, so let's add an alias specifically:
  Names::Add ("clientDevice", devices.Get (0));  // 0 is client

  // Demonstrate renaming a node
  Ptr<Node> oldServer = Names::Find<Node> ("server");
  Names::Rename ("server", "mighty-server");
  // You can still get it using the new name
  NS_ASSERT (Names::Find<Node> ("mighty-server") == oldServer);

  // Install Internet stack, using Names as well
  InternetStackHelper internet;
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      internet.Install (nodes.Get (i));
    }

  // Assign IP addresses to devices
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Install UDP Echo Server on the "mighty-server" node (was "server")
  uint16_t port = 9;
  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps =
    echoServer.Install (Names::Find<Node> ("mighty-server"));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  // Install UDP Echo Client on the "client" node
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port); // server is node 1
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));

  ApplicationContainer clientApps =
    echoClient.Install (Names::Find<Node> ("client"));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable pcap tracing on all devices; use object names for path
  csma.EnablePcapAll ("object-names-csma", true);

  // Enable ascii tracing as well for demonstration
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("object-names-trace.tr"));

  // Emphasize retrieving and modifying attributes via the names service/mix with config
  Config::Set ("/Names/peer2Device/Mtu", UintegerValue (1200));
  Names::Add ("peer2Device", devices.Get (2));

  // Run simulation
  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}