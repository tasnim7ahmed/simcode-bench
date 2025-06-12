#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
  CommandLine cmd;
  cmd.Parse(argc, argv);

  Config::SetDefault ("ns3::UdpClient::Interval", TimeValue (MilliSeconds (10)));
  Config::SetDefault ("ns3::UdpClient::MaxPackets", UintegerValue (1000));
  Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (1024));

  NodeContainer gnbNodes;
  gnbNodes.Create(1);

  NodeContainer ueNodes;
  ueNodes.Create(2);

  MobilityHelper mobilityGnb;
  mobilityGnb.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobilityGnb.Install(gnbNodes);
  Ptr<ConstantPositionMobilityModel> gnb0Pos = gnbNodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
  gnb0Pos->SetPosition(Vector(25.0, 25.0, 0.0));

  MobilityHelper mobilityUe;
  mobilityUe.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("0|50|0|50"));
  mobilityUe.Install(ueNodes);

  MmWaveHelper mmwaveHelper;
  mmwaveHelper.SetSchedulerType ("ns3::RrMmWaveMacScheduler");

  NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnbDevice(gnbNodes);
  NetDeviceContainer ueDevs = mmwaveHelper.InstallUeDevice(ueNodes);

  mmwaveHelper.AddX2Interface(gnbNodes);

  InternetStackHelper internet;
  internet.Install(gnbNodes);
  internet.Install(ueNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);
  Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

  Ipv4Address serverAddress = ueIpIface.GetAddress(1,0);

  UdpServerHelper server(5001);
  ApplicationContainer serverApp = server.Install(ueNodes.Get(1));
  serverApp.Start(Seconds(1.0));
  serverApp.Stop(Seconds(10.0));

  UdpClientHelper client(serverAddress, 5001);
  ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
  clientApp.Start(Seconds(1.0));
  clientApp.Stop(Seconds(10.0));

  mmwaveHelper.EnableTraces();

  Simulator::Stop(Seconds(10.0));

  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll();

  Simulator::Run();

  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier());
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
      std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
      std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
      std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024  << " Mbps\n";
    }

  monitor->SerializeToXmlFile("mmwave-udp-flowmon.xml", true, true);

  Simulator::Destroy();
  return 0;
}