#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Vanet80211pSimulation");

uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;
double totalDelay = 0.0;

void RxPacket(Ptr<const Packet> packet, const Address &address)
{
  packetsReceived++;
}

void ServerRxCallback(Ptr<Socket> socket)
{
  Address from;
  Ptr<Packet> packet;
  while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() > 0)
        {
          packetsReceived++;
        }
    }
}

void ClientTxCallback(Ptr<const Packet> packet)
{
  packetsSent++;
}

void ServerTraceDelay(Ptr<Packet> p, const Address &address)
{
  Time delay = Simulator::Now() - p->GetUid();
  totalDelay += delay.GetSeconds();
}

int main(int argc, char *argv[])
{
  uint32_t numVehicles = 5;
  double simDuration = 20.0;
  double distance = 50.0;
  uint16_t port = 4000;
  double packetInterval = 1.0;
  uint32_t packetSize = 256;
  uint32_t maxPackets = (uint32_t)(simDuration / packetInterval);

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NodeContainer vehicles;
  vehicles.Create(numVehicles);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  Wifi80211pHelper wifi = Wifi80211pHelper::Default();
  NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                              "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                              "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

  NetDeviceContainer deviceContainer = wifi.Install(wifiPhy, mac, vehicles);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      positionAlloc->Add(Vector(i * distance, 0.0, 0.0));
    }
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  mobility.Install(vehicles);

  // Assign speeds (varying but all in x dir)
  std::vector<double> speeds = {20.0, 22.5, 18.0, 25.0, 21.0}; // m/s
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      Ptr<MobilityModel> mob = vehicles.Get(i)->GetObject<MobilityModel>();
      Ptr<ConstantVelocityMobilityModel> cvMob = DynamicCast<ConstantVelocityMobilityModel>(mob);
      cvMob->SetVelocity(Vector(speeds[i], 0.0, 0.0));
    }

  InternetStackHelper internet;
  internet.Install(vehicles);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign(deviceContainer);

  // Set up UDP server on vehicle 0
  UdpServerHelper server(port);
  ApplicationContainer serverApps = server.Install(vehicles.Get(0));
  serverApps.Start(Seconds(0.0));
  serverApps.Stop(Seconds(simDuration));

  // Set up UDP clients on others
  ApplicationContainer clientApps;
  for (uint32_t i = 1; i < numVehicles; ++i)
    {
      UdpClientHelper client(interfaces.GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
      client.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
      client.SetAttribute("PacketSize", UintegerValue(packetSize));
      ApplicationContainer app = client.Install(vehicles.Get(i));
      app.Start(Seconds(1.0));
      app.Stop(Seconds(simDuration));
      clientApps.Add(app);
    }

  Simulator::Schedule(Seconds(0.1), [&]() {
    for (uint32_t i = 1; i < numVehicles; ++i)
      {
        Ptr<UdpClient> client = DynamicCast<UdpClient>(clientApps.Get(i - 1));
        client->TraceConnectWithoutContext("Tx", MakeCallback(&ClientTxCallback));
      }
  });

  // Attach packet receive counter for server
  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApps.Get(0));
  udpServer->TraceConnectWithoutContext("Rx", MakeCallback([](Ptr<const Packet> packet, const Address &address) {
    packetsReceived++;
  }));

  // NetAnim setup
  AnimationInterface anim("vanet-80211p.xml");
  for (uint32_t i = 0; i < numVehicles; ++i)
    {
      anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i));
      anim.UpdateNodeColor(vehicles.Get(i), 0, 255, 0); // green
    }
  anim.EnablePacketMetadata(true);
  anim.EnableWifiMacCounters(Seconds(0), Seconds(simDuration));
  anim.EnableWifiPhyCounters(Seconds(0), Seconds(simDuration));

  // FlowMonitor for e2e delay and PDR
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(simDuration));
  Simulator::Run();

  flowmon->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = flowmon->GetFlowStats();
  uint64_t totalTxPackets = 0;
  uint64_t totalRxPackets = 0;
  double sumDelay = 0.0;
  for (auto const &flow : stats)
    {
      totalTxPackets += flow.second.txPackets;
      totalRxPackets += flow.second.rxPackets;
      sumDelay += flow.second.delaySum.GetSeconds();
    }
  double pdr = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) * 100.0 : 0.0;
  double avgDelay = (totalRxPackets > 0) ? (sumDelay / totalRxPackets) : 0.0;

  std::cout << "VANET 802.11p Simulation Results" << std::endl;
  std::cout << "-------------------------------" << std::endl;
  std::cout << "Total packets sent:     " << totalTxPackets << std::endl;
  std::cout << "Total packets received: " << totalRxPackets << std::endl;
  std::cout << "Packet Delivery Ratio:  " << pdr << " %" << std::endl;
  std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

  Simulator::Destroy();
  return 0;
}