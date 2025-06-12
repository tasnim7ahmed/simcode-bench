#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/packet.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWifiMultipleNetworks");

struct Stats
{
  uint64_t totalBytes;
  Time txopStart;
  Time txopEnd;
  std::vector<Time> txopDurations;
  Stats() : totalBytes(0) {}
};

void
TxopTrace(std::string context, Ptr<const Packet> pkt, WifiMacHeader const &hdr, WifiPhyTag const &tag, Stats *stats)
{
  if (hdr.IsQosData())
    {
      AccessClass ac = hdr.GetQosTid() == 5 ? AC_VI : AC_BE;
      if (stats->txopStart == Seconds(0))
        {
          stats->txopStart = Simulator::Now();
        }
      stats->txopEnd = Simulator::Now();
    }
}

// OnEndTx handler to measure TXOP durations per AC
void
PhyTxEndTrace(std::string context, Stats *acStats)
{
  if (acStats->txopStart != Seconds(0))
    {
      Time duration = Simulator::Now() - acStats->txopStart;
      acStats->txopDurations.push_back(duration);
      acStats->txopStart = Seconds(0);
    }
}

void
RxAppRx(std::string context, Ptr<const Packet> packet, const Address &addr, Stats *stats)
{
  stats->totalBytes += packet->GetSize();
}

int
main(int argc, char *argv[])
{
  bool enablePcap = false;
  double distance = 5.0;
  uint32_t payloadSize = 1472;
  double simTime = 10.0;
  bool rtsCts = false;

  CommandLine cmd;
  cmd.AddValue("distance", "Distance between STA and AP", distance);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("rtsCts", "Enable RTS/CTS", rtsCts);
  cmd.AddValue("pcap", "Enable PCAP capture", enablePcap);
  cmd.AddValue("simTime", "Simulation time", simTime);
  cmd.Parse(argc, argv);

  // Global statistics holders
  Stats stats[4][2]; // [network][0=AC_BE,1=AC_VI]
  std::vector<Ptr<UdpServer>> servers(4);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211ac);

  WifiMacHelper mac;

  QosWifiMacHelper qosMac = QosWifiMacHelper::Default ();

  NodeContainer aps, stas;
  aps.Create(4);
  stas.Create(4);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  // AP positions
  positionAlloc->Add(Vector(0, 0, 0)); // AP0
  positionAlloc->Add(Vector(0, 20, 0)); // AP1
  positionAlloc->Add(Vector(0, 40, 0)); // AP2
  positionAlloc->Add(Vector(0, 60, 0)); // AP3
  // STA positions
  positionAlloc->Add(Vector(distance, 0, 0)); // STA0
  positionAlloc->Add(Vector(distance, 20, 0)); // STA1
  positionAlloc->Add(Vector(distance, 40, 0)); // STA2
  positionAlloc->Add(Vector(distance, 60, 0)); // STA3

  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (aps);
  mobility.Install (stas);

  // Set different channels (logical) for each Wi-Fi network
  phy.Set("ChannelNumber", UintegerValue(36)); // UNII-1 channel 36, but since sharing the physical, logical separation by SSID only

  NetDeviceContainer apDevs[4], staDevs[4];

  Ssid ssids[4] = {Ssid("wifi-net0"), Ssid("wifi-net1"), Ssid("wifi-net2"), Ssid("wifi-net3")};

  // For RTS/CTS
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(rtsCts ? "0" : "2200"));

  for (uint32_t i = 0; i < 4; ++i)
    {
      // AP
      qosMac.SetType ("ns3::ApWifiMac",
            "Ssid", SsidValue (ssids[i]),
            "QosSupported", BooleanValue(true));
      apDevs[i] = wifi.Install (phy, qosMac, NodeContainer (aps.Get(i)));

      // STA
      qosMac.SetType ("ns3::StaWifiMac",
            "Ssid", SsidValue (ssids[i]),
            "ActiveProbing", BooleanValue (false),
            "QosSupported", BooleanValue(true));
      staDevs[i] = wifi.Install (phy, qosMac, NodeContainer (stas.Get(i)));
    }

  InternetStackHelper stack;
  stack.Install (aps);
  stack.Install (stas);

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer apIfs[4], staIfs[4];
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << i << ".0";
      address.SetBase (Ipv4Address(subnet.str().c_str()), "255.255.255.0");
      staIfs[i] = address.Assign (staDevs[i]);
      address.NewNetwork ();
      apIfs[i] = address.Assign (apDevs[i]);
      address.NewNetwork ();
    }

  uint16_t portBase = 5000;

  // Application setup, Traffic for BE and VI per network
  for (uint32_t i = 0; i < 4; ++i)
    {
      // One UDP server per AP for AC_BE, one per AP for AC_VI
      for (uint32_t acIdx = 0; acIdx < 2; ++acIdx)
        {
          uint16_t port = portBase + i*2 + acIdx;
          Ptr<UdpServer> server = CreateObject<UdpServer> ();
          servers[i] = server;
          aps.Get(i)->AddApplication(server);
          server->SetStartTime(Seconds(0.0));
          server->SetStopTime(Seconds(simTime+1));

          server->Setup(port);

          // Trace reception for throughput statistics
          std::ostringstream context;
          context << "ap-" << i << "-ac-" << acIdx;
          server->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxAppRx, &stats[i][acIdx]));
        }
      // OnOff traffic generator for BE (tid=0) and VI (tid=5)
      for (uint32_t acIdx = 0; acIdx < 2; ++acIdx)
        {
          uint16_t port = portBase + i*2 + acIdx;
          uint8_t tid = acIdx == 0 ? 0 : 5;
          ApplicationContainer onoff;
          OnOffHelper onoffHelper("ns3::UdpSocketFactory", InetSocketAddress(apIfs[i].GetAddress(0), port));
          onoffHelper.SetConstantRate(DataRate("10Mbps"), payloadSize);
          onoffHelper.SetAttribute("StartTime", TimeValue(Seconds(0.1)));
          onoffHelper.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

          // Use QosTag to set 802.11e TID for traffic (acIdx==0: AC_BE, acIdx==1: AC_VI)
          onoffHelper.SetAttribute("QosType", UintegerValue(tid));

          onoff.Add(onoffHelper.Install(stas.Get(i)));
        }
    }

  // TXOP tracing
  for (uint32_t i = 0; i < 4; ++i)
    {
      for (uint32_t acIdx = 0; acIdx < 2; ++acIdx)
        {
          std::string devPath = "/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
              "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/Tx";
          Config::ConnectWithoutContext (devPath, MakeBoundCallback (&TxopTrace, &stats[i][acIdx]));

          std::string phyTxEndPath = "/NodeList/" + std::to_string(stas.Get(i)->GetId()) +
              "/DeviceList/0/Phy/PhyTxEnd";
          Config::ConnectWithoutContext (phyTxEndPath, MakeBoundCallback (&PhyTxEndTrace, &stats[i][acIdx]));
        }
    }

  if (enablePcap)
    {
      for (uint32_t i = 0; i < 4; ++i)
        {
          phy.EnablePcap ("qoswifi-ap"+std::to_string(i), apDevs[i].Get(0));
          phy.EnablePcap ("qoswifi-sta"+std::to_string(i), staDevs[i].Get(0));
        }
    }

  Simulator::Stop(Seconds(simTime + 0.5));
  Simulator::Run();

  std::cout << "================== Results ==================" << std::endl;
  for (uint32_t i = 0; i < 4; ++i)
    {
      for (uint32_t acIdx = 0; acIdx < 2; ++acIdx)
        {
          std::string acLabel = acIdx == 0 ? "AC_BE" : "AC_VI";
          double throughput = stats[i][acIdx].totalBytes * 8.0 / (simTime*1000000.0); // Mbps
          std::cout << "Net" << i << " " << acLabel << ": " << throughput << " Mbps, ";
          double avgTxop = 0;
          if (!stats[i][acIdx].txopDurations.empty())
            {
              double sum = 0;
              for (auto t : stats[i][acIdx].txopDurations) sum += t.GetSeconds();
              avgTxop = sum / stats[i][acIdx].txopDurations.size();
            }
          std::cout << "Average TXOP duration: " << avgTxop << " s" << std::endl;
        }
    }
  Simulator::Destroy();
  return 0;
}