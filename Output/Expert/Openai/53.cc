#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include <map>
#include <vector>
#include <iomanip>

using namespace ns3;

struct NetConfig
{
  std::string ssid;
  uint16_t channel;
  bool enableAmpdu;
  uint32_t ampduSize;
  bool enableAmsdu;
  uint32_t amsduSize;
};

struct TracedTxop
{
  double maxDuration;
};

std::map<uint32_t, TracedTxop> apTxopTraceMap;

CommandLine cmd;
double distance = 5.0;
bool enableRts = false;
double txopDurationUs = 0.0;

void
TxopTrace(uint32_t netIdx, WifiMacHeader const &header, Time txop)
{
  if (txop.GetMicroSeconds() > apTxopTraceMap[netIdx].maxDuration)
    {
      apTxopTraceMap[netIdx].maxDuration = txop.GetMicroSeconds();
    }
}

int
main(int argc, char *argv[])
{
  cmd.AddValue("distance", "AP-STA distance in meters", distance);
  cmd.AddValue("enableRts", "Enable RTS/CTS on all stations", enableRts);
  cmd.AddValue("txopDuration", "TXOP duration (us). 0=default", txopDurationUs);
  cmd.Parse(argc, argv);

  Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(enableRts ? 0 : 65535));
  if (txopDurationUs > 0)
    {
      Config::SetDefault("ns3::EdcaTxopN::TxopLimit", TimeValue(MicroSeconds(txopDurationUs)));
    }

  std::vector<NetConfig> nets;
  nets.push_back({"ssidA", 36, true, 65535, false, 0});    // Default A-MPDU 65kB
  nets.push_back({"ssidB", 40, false, 0, false, 0});       // No aggregation
  nets.push_back({"ssidC", 44, false, 0, true, 8192});     // A-MSDU only 8kB
  nets.push_back({"ssidD", 48, true, 32768, true, 4096});  // Both, A-MPDU 32kB, A-MSDU 4kB

  uint32_t nNets = nets.size();
  std::vector<NodeContainer> apNodes, staNodes;
  std::vector<NetDeviceContainer> apDevs, staDevs;
  std::vector<Ipv4InterfaceContainer> apIfaces, staIfaces;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  for (uint32_t i = 0; i < nNets; ++i)
    {
      apNodes.emplace_back();
      apNodes[i].Create(1);
      staNodes.emplace_back();
      staNodes[i].Create(1);

      YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
      phy.SetChannel (wifiChannel.Create ());

      WifiHelper wifi;
      wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

      WifiMacHelper mac;
      Ssid ssid = Ssid(nets[i].ssid);

      wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("HtMcs7"),
                                  "ControlMode", StringValue("HtMcs0"));

      mac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(ssid));

      apDevs.emplace_back(wifi.Install(phy, mac, apNodes[i]));

      mac.SetType("ns3::StaWifiMac",
                  "Ssid", SsidValue(ssid),
                  "ActiveProbing", BooleanValue(false));

      staDevs.emplace_back(wifi.Install(phy, mac, staNodes[i]));

      if (!nets[i].enableAmpdu)
        {
          Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId ()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/MPDUAggregator",
                      PointerValue(0));
        }
      else
        {
          std::ostringstream path;
          path << "/NodeList/" << staNodes[i].Get(0)->GetId()
               << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/MPDUAggregator";
          Config::Set(path.str() + ".AmsduSupported", BooleanValue(false));
          Config::Set(path.str() + ".MaxAmpduSize", UintegerValue(nets[i].ampduSize));
        }

      if (!nets[i].enableAmsdu)
        {
          Config::Set("/NodeList/" + std::to_string(staNodes[i].Get(0)->GetId ()) +
                      "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduAggregator",
                      PointerValue(0));
        }
      else
        {
          std::ostringstream path;
          path << "/NodeList/" << staNodes[i].Get(0)->GetId()
               << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::StaWifiMac/BE_EdcaTxopN/AmsduAggregator";
          Config::Set(path.str() + ".MaxAmsduSize", UintegerValue(nets[i].amsduSize));
        }

      // Set 5 GHz channel
      phy.Set("ChannelNumber", UintegerValue(nets[i].channel));
    }

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

  for (uint32_t i = 0; i < nNets; ++i)
    {
      Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator> ();
      posAlloc->Add(Vector(i*20, 0.0, 0.0));
      posAlloc->Add(Vector(i*20 + distance, 0.0, 0.0));
      mobility.SetPositionAllocator(posAlloc);

      mobility.Install(apNodes[i]);
      mobility.Install(staNodes[i]);
    }

  InternetStackHelper stack;
  for (uint32_t i = 0; i < nNets; ++i)
    {
      stack.Install(apNodes[i]);
      stack.Install(staNodes[i]);
    }

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < nNets; ++i)
    {
      std::ostringstream subnet;
      subnet << "10.1." << std::to_string(i+1) << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      apIfaces.emplace_back(address.Assign(apDevs[i]));
      staIfaces.emplace_back(address.Assign(staDevs[i]));
    }

  // Set up Udp traffic: Each STA sends to its AP
  uint16_t port = 9;
  ApplicationContainer sources, sinks;
  for (uint32_t i = 0; i < nNets; ++i)
    {
      UdpServerHelper server(port);
      sinks.Add(server.Install(apNodes[i].Get(0)));

      UdpClientHelper client(apIfaces[i].GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(0));
      client.SetAttribute("Interval", TimeValue (MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(1472));
      sources.Add(client.Install(staNodes[i].Get(0)));
    }

  sinks.Start(Seconds(0.5));
  sources.Start(Seconds(1.0));
  sinks.Stop(Seconds(11.0));
  sources.Stop(Seconds(11.0));

  // Throughput statistics
  std::vector<uint64_t> rxBytes(nNets, 0);

  Simulator::Schedule(Seconds(11.01), [&](){
    for (uint32_t i = 0; i < nNets; ++i)
      {
        Ptr<UdpServer> server = DynamicCast<UdpServer>(sinks.Get(i));
        rxBytes[i] = server->GetReceived() * 1472;
      }
  });

  // AP TXOP tracing
  for (uint32_t i = 0; i < nNets; ++i)
    {
      apTxopTraceMap[i].maxDuration = 0.0;
      std::ostringstream path;
      path << "/NodeList/" << apNodes[i].Get(0)->GetId()
           << "/DeviceList/0/$ns3::WifiNetDevice/Mac/$ns3::ApWifiMac/BE_EdcaTxopN/TxopEvent";
      Config::Connect(path.str(),
                      MakeBoundCallback(&TxopTrace, i));
    }

  Simulator::Stop(Seconds(11.02));
  Simulator::Run();

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Network\tChannel\tThroughput (Mbps)\tMax TXOP (us)\n";
  for (uint32_t i = 0; i < nNets; ++i)
    {
      double throughput = static_cast<double>(rxBytes[i]) * 8.0 / 10e6;
      std::cout << nets[i].ssid << "\t"
                << nets[i].channel << "\t"
                << throughput << "\t\t"
                << apTxopTraceMap[i].maxDuration << "\n";
    }

  Simulator::Destroy();
  return 0;
}