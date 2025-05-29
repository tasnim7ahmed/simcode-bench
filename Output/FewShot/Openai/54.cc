#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAc80p80GoodputSim");

uint64_t lastTotalRx = 0;

static void
CalculateThroughput(Ptr<PacketSink> sink, double &goodput, double lastTime, double &result)
{
    Time now = Simulator::Now();
    double curTime = now.GetSeconds();
    double diff = curTime - lastTime;
    if (diff > 0) {
        uint64_t curRx = sink->GetTotalRx();
        goodput = (curRx - lastTotalRx) * 8.0 / diff / 1e6; // Mbps
        lastTotalRx = curRx;
        result = goodput;
    }
}

int main(int argc, char *argv[])
{
    // Parameters to sweep
    std::vector<std::string> transports = {"UDP", "TCP"};
    std::vector<bool> rtsCtsOpts = {false, true};
    std::vector<uint16_t> channelWidths = {20, 40, 80, 160};
    std::vector<std::string> mcsVec = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    std::vector<bool> shortGiOpts = {false, true};
    std::vector<bool> use80p80 = {false, true};
    double distance = 5.0; // meters; can be set via cmdline

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between STA and AP", distance);
    cmd.Parse(argc, argv);

    // Output CSV
    std::ofstream out("wifi-ac80p80-goodput.csv");
    out << "Transport,RTS_CTS,ChannelWidth,80+80,MCS,ShortGI,Goodput_Mbps" << std::endl;
    
    for (const auto& transport : transports)
    {
        for (const auto& rtsCts : rtsCtsOpts)
        {
            for (const auto& chanWidth : channelWidths)
            {
                for (const auto& is80p80 : use80p80)
                {
                    if (chanWidth != 80 && is80p80) continue; // 80+80 only makes sense for 80 or above
                    for (const auto& mcs : mcsVec)
                    {
                        for (const auto& shortGi : shortGiOpts)
                        {
                            // Clean up before next run
                            Simulator::Destroy();

                            // Nodes
                            NodeContainer wifiStaNode;
                            wifiStaNode.Create(1);
                            NodeContainer wifiApNode;
                            wifiApNode.Create(1);

                            // Mobility
                            MobilityHelper mobility;
                            Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
                            pos->Add(Vector(0.0, 0.0, 0.0));
                            pos->Add(Vector(distance, 0.0, 0.0));
                            mobility.SetPositionAllocator(pos);
                            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                            mobility.Install(wifiApNode);
                            mobility.Install(wifiStaNode);

                            // WiFi PHY+MAC (802.11ac)
                            YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
                            Ptr<YansWifiChannel> chan = channel.Create();
                            SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default();
                            phy.SetChannel(chan);
                            WifiHelper wifi;
                            wifi.SetStandard(WIFI_STANDARD_80211ac);

                            // Configure channel width
                            phy.Set("ChannelWidth", UintegerValue(chanWidth));
                            if (chanWidth == 80 && is80p80)
                                phy.Set("FrequencyBands", StringValue("80+80"));

                            // Guard interval
                            phy.Set("ShortGuardEnabled", BooleanValue(shortGi));

                            // MAC
                            WifiMacHelper mac;
                            Ssid ssid = Ssid("wifi-ac-ssid");

                            // AP
                            mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                            NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

                            // STA
                            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
                            NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

                            // Rate control – force MCS
                            std::ostringstream rspec;
                            rspec << "VhtMcs" << mcs;
                            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(rspec.str()),
                                "ControlMode", StringValue(rspec.str()));

                            // RTS/CTS
                            Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(rtsCts ? "0" : "999999"));

                            // Internet stack
                            InternetStackHelper stack;
                            stack.Install(wifiApNode);
                            stack.Install(wifiStaNode);

                            // IP addressing
                            Ipv4AddressHelper address;
                            address.SetBase("192.168.1.0", "255.255.255.0");
                            Ipv4InterfaceContainer staIf = address.Assign(staDevice);
                            Ipv4InterfaceContainer apIf = address.Assign(apDevice);

                            // Traffic (UDP or TCP)
                            uint16_t port = 9;
                            ApplicationContainer serverApp, clientApp;
                            Ptr<PacketSink> sink;
                            double simulationTime = 5.0; // seconds

                            if (transport == "UDP")
                            {
                                UdpServerHelper server(port);
                                serverApp = server.Install(wifiApNode.Get(0));
                                serverApp.Start(Seconds(0.0));
                                serverApp.Stop(Seconds(simulationTime));

                                UdpClientHelper client(apIf.GetAddress(0), port);
                                client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                                client.SetAttribute("Interval", TimeValue(Time("0.0005s"))); // 2000 pkt/sec
                                client.SetAttribute("PacketSize", UintegerValue(1472));
                                clientApp = client.Install(wifiStaNode.Get(0));
                                clientApp.Start(Seconds(1.0));
                                clientApp.Stop(Seconds(simulationTime));

                                Ptr<UdpServer> udpSrv = DynamicCast<UdpServer>(serverApp.Get(0));
                                sink = udpSrv;
                            }
                            else // TCP
                            {
                                PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                                serverApp = sinkHelper.Install(wifiApNode.Get(0));
                                serverApp.Start(Seconds(0.0));
                                serverApp.Stop(Seconds(simulationTime));
                                
                                OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(apIf.GetAddress(0), port));
                                onoff.SetAttribute("DataRate", DataRateValue(DataRate("1000Mbps")));
                                onoff.SetAttribute("PacketSize", UintegerValue(1472));
                                onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
                                onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
                                clientApp = onoff.Install(wifiStaNode.Get(0));

                                sink = DynamicCast<PacketSink>(serverApp.Get(0));
                            }

                            // Run simulation
                            Simulator::Stop(Seconds(simulationTime));
                            lastTotalRx = 0;
                            double goodput = 0.0, result = 0.0;
                            Simulator::Schedule(Seconds(simulationTime-0.5), &CalculateThroughput, sink, std::ref(goodput), simulationTime-1.0, std::ref(result));
                            Simulator::Run();
                            uint64_t totalRx = sink->GetTotalRx();
                            goodput = totalRx * 8.0 / (simulationTime - 1.0) / 1e6;

                            // Output
                            out << transport << "," << (rtsCts?"1":"0") << "," << chanWidth << "," << (is80p80?"1":"0") << ",";
                            out << mcs << "," << (shortGi?"1":"0") << ",";
                            out << std::fixed << std::setprecision(4) << goodput << std::endl;

                            Simulator::Destroy();
                        }
                    }
                }
            }
        }
    }

    out.close();
    return 0;
}