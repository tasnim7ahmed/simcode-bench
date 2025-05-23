#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model-module.h"
#include "ns3/rng-stream.h"

// NS_LOG_COMPONENT_DEFINE ("TcpPointToPointLoss");

int
main (int argc, char *argv[])
{
  // Allow for logging within the simulation if needed (optional for production code)
  // LogComponentEnable ("TcpPointToPointLoss", LOG_LEVEL_INFO);

  // 1. Create nodes
  ns3::NodeContainer nodes;
  nodes.Create (2);

  // 2. Configure Point-to-Point link
  ns3::PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", ns3::StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", ns3::StringValue ("10ms"));

  // 3. Introduce packet loss (0.1 = 10%)
  ns3::Ptr<ns3::RateErrorModel> em = ns3::CreateObject<ns3::RateErrorModel> ();
  em->SetAttribute ("RanVar", ns3::PointerValue (ns3::CreateObject<ns3::UniformRandomVariable> ()));
  em->SetAttribute ("ErrorRate", ns3::DoubleValue (0.1));
  
  // Apply error model to the receive side of the PointToPointNetDevice.
  // This will affect packets arriving at each device from the channel.
  p2p.SetDeviceAttribute ("ReceiveErrorModel", ns3::PointerValue (em));

  // Install devices on nodes
  ns3::NetDeviceContainer devices = p2p.Install (nodes);

  // 4. Install Internet Stack
  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  // 5. Assign IP Addresses
  ns3::Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // 6. Setup TCP Server (PacketSink)
  uint16_t sinkPort = 9;
  ns3::Address sinkAddress (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), sinkPort));
  ns3::PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkAddress);
  
  // Install server on node 1
  ns3::ApplicationContainer sinkApps = sinkHelper.Install (nodes.Get (1)); 
  sinkApps.Start (ns3::Seconds (0.0)); // Start server at simulation start
  sinkApps.Stop (ns3::Seconds (12.0)); // Stop server slightly after client

  // 7. Setup TCP Client (BulkSendApplication)
  // Client sends to server's IP address and port
  ns3::BulkSendHelper clientHelper ("ns3::TcpSocketFactory",
                                    ns3::InetSocketAddress (interfaces.GetAddress (1), sinkPort));
  
  // Set MaxBytes to 0 to send indefinitely until application is stopped
  clientHelper.SetAttribute ("MaxBytes", ns3::UintegerValue (0)); 
  
  // Install client on node 0
  ns3::ApplicationContainer clientApps = clientHelper.Install (nodes.Get (0));
  clientApps.Start (ns3::Seconds (1.0));  // Start sending after 1 second
  clientApps.Stop (ns3::Seconds (11.0)); // Stop sending after 10 seconds (1.0 + 10.0 = 11.0)

  // Run simulation
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}