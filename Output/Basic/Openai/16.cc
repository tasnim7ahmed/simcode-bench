#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/internet-module.h"
#include "ns3/gnuplot.h"
#include "ns3/on-off-helper.h"
#include "ns3/config-store-module.h"

using namespace ns3;

class Experiment
{
public:
  Experiment(std::string name);
  void SetPosition(Ptr<Node> node, Vector position);
  Vector GetPosition(Ptr<Node> node);
  void MoveNode(Ptr<Node> node, Vector position);
  void ReceivePacket(Ptr<Socket> socket);
  void CreateSockets();
  void Run(std::string wifiManager, std::string dataRate, Gnuplot2dDataset &dataset);

private:
  NodeContainer nodes;
  NetDeviceContainer devices;
  Ptr<PacketSocket> sendSocket;
  Ptr<PacketSocket> recvSocket;
  Ptr<UniformRandomVariable> uv;
  uint64_t totalRx;
  Time lastTime;
};

Experiment::Experiment(std::string name)
{
  uv = CreateObject<UniformRandomVariable>();
  totalRx = 0;
}

void
Experiment::SetPosition(Ptr<Node> node, Vector position)
{
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  mob->SetPosition(position);
}

Vector
Experiment::GetPosition(Ptr<Node> node)
{
  Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
  return mob->GetPosition();
}

void
Experiment::MoveNode(Ptr<Node> node, Vector position)
{
  SetPosition(node, position);
}

void
Experiment::ReceivePacket(Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom(from)))
    {
      totalRx += packet->GetSize();
    }
}

void
Experiment::CreateSockets()
{
  PacketSocketAddress socketAddr;
  socketAddr.SetSingleDevice(devices.Get(1)->GetIfIndex());
  socketAddr.SetPhysicalAddress(devices.Get(1)->GetAddress());
  socketAddr.SetProtocol(0);

  recvSocket = StaticCast<PacketSocket>(
      Socket::CreateSocket(nodes.Get(1), PacketSocketFactory::GetTypeId()));
  recvSocket->Bind(socketAddr);
  recvSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));

  // Send socket
  PacketSocketAddress sendAddr;
  sendAddr.SetSingleDevice(devices.Get(0)->GetIfIndex());
  sendAddr.SetPhysicalAddress(devices.Get(1)->GetAddress());
  sendAddr.SetProtocol(0);
  sendSocket = StaticCast<PacketSocket>(
      Socket::CreateSocket(nodes.Get(0), PacketSocketFactory::GetTypeId()));
  sendSocket->Bind();
  sendSocket->Connect(sendAddr);
}

void
Experiment::Run(std::string wifiManager, std::string dataRate, Gnuplot2dDataset &dataset)
{
  nodes.Create(2);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(10.0, 0.0, 0.0));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(nodes);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager(wifiManager, "DataMode", StringValue(dataRate), "ControlMode", StringValue(dataRate));

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
  wifiMac.SetType("ns3::AdhocWifiMac");

  PacketSocketHelper packetSocket;
  packetSocket.Install(nodes);

  devices = wifi.Install(wifiPhy, wifiMac, nodes);

  InternetStackHelper internet;
  internet.Install(nodes);

  CreateSockets();

  // Traffic generation via OnOffHelper (source: node0, destination: node1, but using packet socket)
  OnOffHelper onoff("ns3::PacketSocketFactory",
                    Address());
  onoff.SetAttribute("PacketSize", UintegerValue(1024));
  onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute("DataRate", DataRateValue(DataRate("5Mbps")));

  PacketSocketAddress dstAddress;
  dstAddress.SetSingleDevice(devices.Get(0)->GetIfIndex());
  dstAddress.SetPhysicalAddress(devices.Get(1)->GetAddress());
  dstAddress.SetProtocol(0);
  ApplicationContainer apps = onoff.Install(nodes.Get(0));
  apps.Start(Seconds(1.0));
  apps.Stop(Seconds(10.0));

  Ptr<Application> app = apps.Get(0);
  app->GetObject<OnOffApplication>()->SetAttribute("Remote", AddressValue(dstAddress));

  totalRx = 0;
  lastTime = Seconds(0);

  // Simulation steps for moving and sampling throughput
  double simTime = 10.0;
  double step = 1.0;
  Vector initPos = GetPosition(nodes.Get(1));

  for (double t = 1.0; t <= simTime; t += step)
    {
      double distance = 5.0 + t * 2.0; // e.g., increase distance linearly
      Vector newPos = Vector(distance, 0.0, 0.0);
      Simulator::Schedule(Seconds(t), &Experiment::MoveNode, this, nodes.Get(1), newPos);
      Simulator::Schedule(Seconds(t+step-0.01), [&]()
      {
        double timeNow = Simulator::Now().GetSeconds();
        double throughput = (totalRx * 8.0) / 1e6 / (timeNow - lastTime.GetSeconds()); // Mbps
        dataset.Add(distance, throughput);
        lastTime = Simulator::Now();
        totalRx = 0;
      });
    }

  Simulator::Stop(Seconds(simTime+1));
  Simulator::Run();
  Simulator::Destroy();
}

int
main(int argc, char *argv[])
{
  std::string wifiManager = "ns3::AarfWifiManager";
  std::string dataRate = "DsssRate11Mbps";

  CommandLine cmd;
  cmd.AddValue("manager", "WiFi remote station manager", wifiManager);
  cmd.AddValue("rate", "WiFi Data Rate", dataRate);
  cmd.Parse(argc, argv);

  std::vector<std::string> managers = {"ns3::AarfWifiManager", "ns3::MinstrelWifiManager"};
  std::vector<std::string> rates = {"DsssRate1Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};

  for (auto m : managers)
    {
      for (auto r : rates)
        {
          std::string graphFile = "adhoc-throughput-" + m.substr(5) + "-" + r + ".plt";
          Gnuplot plot(graphFile);
          plot.SetTitle("Throughput vs Distance (" + m + " " + r + ")");
          plot.SetLegend("Distance (m)", "Throughput (Mbps)");
          Gnuplot2dDataset dataset;
          dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

          Experiment exp("AdHocSim");
          exp.Run(m, r, dataset);

          plot.AddDataset(dataset);
          std::string pngFile = "adhoc-throughput-" + m.substr(5) + "-" + r + ".png";
          plot.SetTerminal("png");
          plot.SetOutputFilename(pngFile);

          std::ofstream plotFile(graphFile);
          plot.GenerateOutput(plotFile);
          plotFile.close();
        }
    }
  return 0;
}