#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);

  // Configure CSMA
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  // Install CSMA devices
  NetDeviceContainer devices = csma.Install (nodes);

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Install TCP server (PacketSink) on node 0 (first node)
  uint16_t port = 8080;
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (0));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (10.0));

  // Install TCP clients (BulkSendApplication) on nodes 1, 2, 3
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", sinkAddress);
  clientHelper.SetAttribute ("MaxBytes", UintegerValue (0)); // Send unlimited data

  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < 4; ++i)
    {
      clientHelper.SetAttribute ("SendSize", UintegerValue (1024));
      clientApps.Add (clientHelper.Install (nodes.Get (i)));
    }
  clientApps.Start (Seconds (1.0));
  clientApps.Stop (Seconds (10.0));

  // Set the TCP send rate to 5 Mbps per client using the RateErrorModel on device queue
  // More modern way is to use TrafficControlHelper and set DataRate on the socket application
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1024));
  Config::SetDefault ("ns3::BulkSendApplication::SendSize", UintegerValue (1024));

  // Limit traffic by TCP send buffer and application send rate
  for (uint32_t i = 1; i < 4; ++i)
    {
      std::ostringstream oss;
      oss << "/NodeList/" << nodes.Get (i)->GetId ()
          << "/ApplicationList/0/$ns3::BulkSendApplication/SendSocket";
      Config::Set (oss.str () + "/DataRate", DataRateValue (DataRate ("5Mbps")));
    }

  // Enable pcap tracing
  csma.EnablePcapAll ("csma-tcp");

  // Run simulation
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}