#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("BasicP2PTcpSimulation");

static uint32_t txPackets[3] = {0, 0, 0};
static uint32_t rxPackets[3] = {0, 0, 0};

void TxCallback (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t iface)
{
  for (uint32_t i = 0; i < 3; ++i)
    {
      if (ipv4 == NodeList::GetNode(i)->GetObject<Ipv4>())
        {
          txPackets[i]++;
          break;
        }
    }
}

void RxCallback (Ptr<const Packet> p, const Address &address, Ptr<Ipv4> ipv4, uint32_t iface)
{
  for (uint32_t i = 0; i < 3; ++i)
    {
      if (ipv4 == NodeList::GetNode(i)->GetObject<Ipv4>())
        {
          rxPackets[i]++;
          break;
        }
    }
}

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (3);

  // Create P2P channels: n0--n1, n1--n2
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (nodes.Get(0), nodes.Get(1));
  NetDeviceContainer d1d2 = p2p.Install (nodes.Get(1), nodes.Get(2));
  
  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = address.Assign (d0d1);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = address.Assign (d1d2);

  // Set up static routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Install applications: TCP Socket (BulkSend) from n0 to n2, and Sink on n2
  uint16_t port = 5000;
  Address sinkAddress (InetSocketAddress (i1i2.GetAddress (1), port));
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get(2));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  BulkSendHelper source ("ns3::TcpSocketFactory", sinkAddress);
  source.SetAttribute ("MaxBytes", UintegerValue (0)); // Send unlimited data
  ApplicationContainer sourceApp = source.Install (nodes.Get(0));
  sourceApp.Start (Seconds (1.0));
  sourceApp.Stop (Seconds (10.0));

  // Enable pcap tracing
  p2p.EnablePcapAll ("basic-p2p-tcp");

  // Packet counting via trace hooks
  Config::Connect ("/NodeList/0/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback ([&](Ptr<const Packet> p){ txPackets[0]++; }));
  Config::Connect ("/NodeList/1/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback ([&](Ptr<const Packet> p){ txPackets[1]++; }));
  Config::Connect ("/NodeList/2/DeviceList/*/$ns3::PointToPointNetDevice/TxQueue/Enqueue", MakeCallback ([&](Ptr<const Packet> p){ txPackets[2]++; }));

  Config::Connect ("/NodeList/0/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback ([&](Ptr<const Packet> p, const Address &address){ rxPackets[0]++; }));
  Config::Connect ("/NodeList/1/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback ([&](Ptr<const Packet> p, const Address &address){ rxPackets[1]++; }));
  Config::Connect ("/NodeList/2/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback ([&](Ptr<const Packet> p, const Address &address){ rxPackets[2]++; }));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  std::cout << "Packet Statistics:\n";
  for (uint32_t i = 0; i < 3; ++i)
    {
      std::cout << "Node " << i << ": "
                << " TxPackets = " << txPackets[i]
                << ", RxPackets = " << rxPackets[i]
                << std::endl;
    }

  Simulator::Destroy ();
  return 0;
}