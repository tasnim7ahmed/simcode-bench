#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/gnuplot.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
  Experiment();
  ~Experiment();

  void SetPosition(Ptr<Node> node, Vector position);
  Vector GetPosition(Ptr<Node> node);
  void MoveNode(Ptr<Node> node, Vector newPosition);
  void ReceivePacket(Ptr<Socket> socket);
  void SetupPacketReception(Ptr<Node> node);

  void Run(uint32_t experimentId, std::string rate, std::string manager);

private:
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ipv4InterfaceContainer interfaces;
  Gnuplot2Ddataset throughputDataset;
};

Experiment::Experiment() {
  throughputDataset = Gnuplot2Ddataset();
}

Experiment::~Experiment() {}

void Experiment::SetPosition(Ptr<Node> node, Vector position) {
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  mobility->SetPosition(position);
}

Vector Experiment::GetPosition(Ptr<Node> node) {
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
  return mobility->GetPosition();
}

void Experiment::MoveNode(Ptr<Node> node, Vector newPosition) {
  SetPosition(node, newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from))) {
    NS_LOG_INFO("Received packet of size " << packet->GetSize());
  }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
  TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket(node, tid);
  PacketSocketAddress localAddress;
  localAddress.SetPhysicalAddress(Address());
  localAddress.SetProtocol(1);
  recvSink->Bind(localAddress);
  recvSink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(uint32_t experimentId, std::string rate, std::string manager) {
  nodes.Create(2);

  WifiMacHelper wifiMac;
  WifiPhyHelper wifiPhy;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  wifi.SetRemoteStationManager(manager, "DataMode", StringValue(rate), "ControlMode", StringValue(rate));

  wifiPhy.SetChannel(WifiChannelHelper::Default());

  wifiMac.SetType("ns3::AdhocWifiMac");
  devices = wifi.Install(wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(nodes);

  PacketSocketHelper packetSocket;
  packetSocket.Install(nodes);

  for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    SetupPacketReception(nodes.Get(i));
  }

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.0.0.0", "255.255.255.0");
  interfaces = address.Assign(devices);

  OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", StringValue("1Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(1000));

  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  AsciiTraceHelper asciiTraceHelper;
  wifiPhy.EnableAsciiAll(asciiTraceHelper.CreateFileStream("experiment-" + std::to_string(experimentId) + ".tr"));

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  double throughput = 0.0; // Placeholder for actual calculation
  throughputDataset.Add(experimentId, throughput);

  Gnuplot gnuplot("experiment-" + std::to_string(experimentId) + ".png");
  gnuplot.SetTitle("Throughput vs Experiment ID");
  gnuplot.SetTerminal("png");
  gnuplot.AddDataset(throughputDataset);

  std::ofstream plotFile("experiment-" + std::to_string(experimentId) + ".plt");
  gnuplot.GenerateOutput(plotFile);
  plotFile.close();

  Simulator::Destroy();
}

int main(int argc, char *argv[]) {
  LogComponentEnable("Experiment", LOG_LEVEL_INFO);

  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  std::vector<std::pair<std::string, std::string>> configs = {
      {"OfdmRate6Mbps", "ns3::AarfcdWifiManager"},
      {"OfdmRate9Mbps", "ns3::AmrrWifiManager"},
      {"OfdmRate12Mbps", "ns3::MinstrelWifiManager"}};

  for (uint32_t i = 0; i < configs.size(); ++i) {
    Experiment experiment;
    experiment.Run(i, configs[i].first, configs[i].second);
  }

  return 0;
}