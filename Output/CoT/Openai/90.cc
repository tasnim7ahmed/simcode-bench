#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleErrorModel");

void PacketReceptionTrace(std::string context, Ptr<const Packet> packet, const Address &address)
{
  static std::ofstream traceFile("simple-error-model.tr", std::ios_base::app);
  traceFile << Simulator::Now().GetSeconds() << " " << context << " " << *packet << std::endl;
}

void QueueTrace (std::string context, uint32_t oldVal, uint32_t newVal)
{
  static std::ofstream traceFile("simple-error-model.tr", std::ios_base::app);
  traceFile << Simulator::Now().GetSeconds() << " " << context << " QlenOld: " << oldVal << " QlenNew: " << newVal << std::endl;
}

int main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);
  LogComponentEnable("SimpleErrorModel", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create(4); // n0, n1, n2, n3

  NodeContainer n0n2(nodes.Get(0), nodes.Get(2));
  NodeContainer n1n2(nodes.Get(1), nodes.Get(2));
  NodeContainer n2n3(nodes.Get(2), nodes.Get(3));

  // Point-to-point links
  PointToPointHelper p2p_n0n2;
  p2p_n0n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n0n2.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p_n0n2.SetQueue("ns3::DropTailQueue");

  PointToPointHelper p2p_n1n2;
  p2p_n1n2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  p2p_n1n2.SetChannelAttribute("Delay", StringValue("2ms"));
  p2p_n1n2.SetQueue("ns3::DropTailQueue");

  PointToPointHelper p2p_n2n3;
  p2p_n2n3.SetDeviceAttribute("DataRate", StringValue("1.5Mbps"));
  p2p_n2n3.SetChannelAttribute("Delay", StringValue("10ms"));
  p2p_n2n3.SetQueue("ns3::DropTailQueue");

  // Install devices
  NetDeviceContainer d_n0n2 = p2p_n0n2.Install(n0n2);
  NetDeviceContainer d_n1n2 = p2p_n1n2.Install(n1n2);
  NetDeviceContainer d_n2n3 = p2p_n2n3.Install(n2n3);

  // Error Model: Rate Error Model on n2->n3 (n2's side)
  Ptr<RateErrorModel> rateError = CreateObject<RateErrorModel>();
  rateError->SetAttribute("ErrorRate", DoubleValue(0.001));
  d_n2n3.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(rateError));

  // Error Model: Burst Error Model on n1->n2 (n2's side)
  Ptr<BurstErrorModel> burstError = CreateObject<BurstErrorModel>();
  burstError->SetAttribute("ErrorRate", DoubleValue(0.01));
  burstError->SetAttribute("BurstSize", UintegerValue(2));
  d_n1n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(burstError));

  // Error Model: List Error Model on n0->n2 (n2's side)
  Ptr<ListErrorModel> listError = CreateObject<ListErrorModel>();
  std::list<uint32_t> pktIds;
  pktIds.push_back(11);
  pktIds.push_back(17);
  listError->SetList(pktIds);
  d_n0n2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(listError));

  // Install Internet stack
  InternetStackHelper stack;
  stack.Install(nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n0n2 = address.Assign(d_n0n2);

  address.SetBase("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n1n2 = address.Assign(d_n1n2);

  address.SetBase("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i_n2n3 = address.Assign(d_n2n3);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // UDP Flow 1: n0 -> n3
  uint16_t udpPort1 = 8000;
  UdpServerHelper udpServer1(udpPort1);
  ApplicationContainer serverApp1 = udpServer1.Install(nodes.Get(3));
  serverApp1.Start(Seconds(0.0));
  serverApp1.Stop(Seconds(2.0));

  UdpClientHelper udpClient1(i_n2n3.GetAddress(1), udpPort1);
  udpClient1.SetAttribute("MaxPackets", UintegerValue(1000000));
  udpClient1.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient1.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApp1 = udpClient1.Install(nodes.Get(0));
  clientApp1.Start(Seconds(0.01));
  clientApp1.Stop(Seconds(2.0));

  // UDP Flow 2: n3 -> n1
  uint16_t udpPort2 = 8001;
  UdpServerHelper udpServer2(udpPort2);
  ApplicationContainer serverApp2 = udpServer2.Install(nodes.Get(1));
  serverApp2.Start(Seconds(0.0));
  serverApp2.Stop(Seconds(2.0));

  UdpClientHelper udpClient2(i_n1n2.GetAddress(0), udpPort2);
  udpClient2.SetAttribute("MaxPackets", UintegerValue(1000000));
  udpClient2.SetAttribute("Interval", TimeValue(Seconds(0.00375)));
  udpClient2.SetAttribute("PacketSize", UintegerValue(210));
  ApplicationContainer clientApp2 = udpClient2.Install(nodes.Get(3));
  clientApp2.Start(Seconds(0.02));
  clientApp2.Stop(Seconds(2.0));

  // FTP/TCP flow: n0 -> n3
  uint16_t ftpPort = 21;
  Address ftpAddress(InetSocketAddress(i_n2n3.GetAddress(1), ftpPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", ftpAddress);
  ApplicationContainer ftpServer = packetSinkHelper.Install(nodes.Get(3));
  ftpServer.Start(Seconds(0.0));
  ftpServer.Stop(Seconds(2.0));

  OnOffHelper ftpClient("ns3::TcpSocketFactory", ftpAddress);
  ftpClient.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
  ftpClient.SetAttribute("PacketSize", UintegerValue(1024));
  ftpClient.SetAttribute("StartTime", TimeValue(Seconds(1.2)));
  ftpClient.SetAttribute("StopTime", TimeValue(Seconds(1.35)));
  ApplicationContainer ftpClientApp = ftpClient.Install(nodes.Get(0));

  // Queue and packet reception tracing
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::PointToPointNetDevice/MacRx", MakeCallback(&PacketReceptionTrace));
  Config::Connect("/NodeList/*/DeviceList/*/TxQueue/Drop", MakeBoundCallback(&QueueTrace));
  Config::Connect("/NodeList/*/DeviceList/*/TxQueue/PacketsInQueue", MakeBoundCallback(&QueueTrace));

  // Clean and create trace file
  {
    std::ofstream traceFile("simple-error-model.tr");
    traceFile << "# NS-3 Simple Error Model Simulation Trace\n";
  }

  Simulator::Stop(Seconds(2.01));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}