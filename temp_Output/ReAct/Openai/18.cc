#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-address.h"
#include "ns3/trace-helper.h"

using namespace ns3;

void
MoveAp(Ptr<Node> apNode, Ptr<ConstantPositionMobilityModel> mob, double step, double separ, double simTime)
{
  int nSteps = static_cast<int>(simTime/separ);
  for (int i = 1; i <= nSteps; ++i)
    {
      Simulator::Schedule(Seconds(i*separ), [mob, step, i]()
      {
        Vector pos = mob->GetPosition();
        mob->SetPosition(Vector(pos.x + step, pos.y, pos.z));
      });
    }
}

void
PhyRxOkTrace (std::string context, Ptr<const Packet> packet, double snr, WifiMode mode, enum WifiPreamble preamble)
{
}

void
PhyRxErrorTrace (std::string context, Ptr<const Packet> packet, double snr)
{
}

void
PhyStateTrace(std::string context, Time start, Time duration, WifiPhyState state)
{
}

void
TxTrace(std::string context, Ptr<const Packet> pkt)
{
}

void
RxTrace(std::string context, Ptr<const Packet> pkt)
{
}

int
main(int argc, char *argv[])
{
  double step = 5.0;
  double separ = 1.0;
  double simTime = 44.0;

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create(2);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211g);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue("ErpOfdmRate6Mbps"),
    "ControlMode", StringValue("ErpOfdmRate6Mbps"));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-ssid");

  mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
  NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

  mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 1.0, 0.0));
  positionAlloc->Add(Vector(0.0, -1.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes);

  MobilityHelper apMobility;
  apMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  apMobility.SetPositionAllocator("ns3::ListPositionAllocator",
                                  "PositionList", VectorValue({Vector(0.0,0.0,0.0)}));
  apMobility.Install(wifiApNode);

  Ptr<ConstantPositionMobilityModel> apMob =
    wifiApNode.Get(0)->GetObject<ConstantPositionMobilityModel>();

  MoveAp(wifiApNode.Get(0), apMob, step, separ, simTime);

  PacketSocketHelper packetSocket;
  packetSocket.Install(wifiStaNodes);
  packetSocket.Install(wifiApNode);

  // Setup addresses for sockets
  PacketSocketAddress staAddr;
  staAddr.SetSingleDevice(staDevices.Get(0)->GetIfIndex());
  staAddr.SetPhysicalAddress(staDevices.Get(1)->GetAddress());
  staAddr.SetProtocol(0);

  PacketSocketAddress staAddr2;
  staAddr2.SetSingleDevice(staDevices.Get(1)->GetIfIndex());
  staAddr2.SetPhysicalAddress(staDevices.Get(0)->GetAddress());
  staAddr2.SetProtocol(0);

  // Setup OnOff data applications for bidirectional traffic at 500kbps.
  ApplicationContainer apps;
  OnOffHelper onoff1("ns3::PacketSocketFactory", Address(staAddr));
  onoff1.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
  onoff1.SetAttribute("PacketSize", UintegerValue(512));
  onoff1.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
  onoff1.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

  OnOffHelper onoff2("ns3::PacketSocketFactory", Address(staAddr2));
  onoff2.SetAttribute("DataRate", DataRateValue(DataRate("500kbps")));
  onoff2.SetAttribute("PacketSize", UintegerValue(512));
  onoff2.SetAttribute("StartTime", TimeValue(Seconds(0.5)));
  onoff2.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

  apps.Add(onoff1.Install(wifiStaNodes.Get(0)));
  apps.Add(onoff2.Install(wifiStaNodes.Get(1)));

  // Tracing
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&TxTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&RxTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State", MakeCallback(&PhyStateTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxOk", MakeCallback(&PhyRxOkTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxError", MakeCallback(&PhyRxErrorTrace));

  Simulator::Stop(Seconds(simTime));
  Simulator::Run();
  Simulator::Destroy();
  return 0;
}