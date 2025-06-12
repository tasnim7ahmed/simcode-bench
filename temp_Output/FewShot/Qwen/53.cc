#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationSimulation");

class NetworkConfig {
public:
    std::string name;
    uint16_t channelNumber;
    bool enableAmpdu;
    uint32_t ampduMaxSize;
    bool enableAmsdu;
    uint32_t amsduMaxSize;
    double distance;
    bool enableRtsCts;
    uint32_t txopDurationUs;
};

void PrintTxop(Ptr<const WifiNetDevice> dev, Time duration) {
    NS_LOG_INFO("TXOP Duration on " << dev->GetAddress() << ": " << duration.As(Time::US));
}

int main(int argc, char *argv[]) {
    LogComponentEnable("WifiAggregationSimulation", LOG_LEVEL_INFO);

    // Default parameters that can be overridden via command-line
    std::vector<NetworkConfig> configs = {
        {"Network-A", 36, true, 65535, false, 0, 1.0, false, 0},
        {"Network-B", 40, false, 0, false, 0, 1.0, false, 0},
        {"Network-C", 44, false, 0, true, 8192, 1.0, false, 0},
        {"Network-D", 48, true, 32768, true, 4096, 1.0, false, 0}
    };

    CommandLine cmd;
    for (size_t i = 0; i < configs.size(); ++i) {
        std::string prefix = "config" + std::to_string(i + 1);
        cmd.AddValue(prefix + "Distance", "Distance between AP and STA (m)", configs[i].distance);
        cmd.AddValue(prefix + "RtsCts", "Enable RTS/CTS", configs[i].enableRtsCts);
        cmd.AddValue(prefix + "TxopDurationUs", "TXOP Duration in microseconds", configs[i].txopDurationUs);
    }
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    NodeContainer staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5MHZ);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (size_t i = 0; i < 4; ++i) {
        NS_LOG_INFO("Setting up network: " << configs[i].name);

        Ssid ssid = Ssid("wifi-network-" + std::to_string(configs[i].channelNumber));
        WifiMacHelper mac;
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        NetDeviceContainer apDevice = wifi.Install(phy, mac, apNodes.Get(i));
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
        NetDeviceContainer staDevice = wifi.Install(phy, mac, staNodes.Get(i));

        Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/ShortRetryLimit", UintegerValue(10));
        Config::Set("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/LongRetryLimit", UintegerValue(10));

        if (configs[i].enableAmpdu) {
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/amsduSupport", BooleanValue(false));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/mpduAggregator", StringValue("ns3::AmpduAggregator"));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/maxAmpduSize", UintegerValue(configs[i].ampduMaxSize));
        }

        if (configs[i].enableAmsdu && !configs[i].enableAmpdu) {
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/ampduSupport", BooleanValue(false));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/amsduSupport", BooleanValue(true));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/amsduAggregator", StringValue("ns3::AmsduAggregator"));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/maxAmsduSize", UintegerValue(configs[i].amsduMaxSize));
        }

        if (configs[i].enableRtsCts) {
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/UseRtsForUnicast", BooleanValue(true));
        }

        if (configs[i].txopDurationUs > 0) {
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop/queue", StringValue("ns3::DropTailQueue<Packet>"));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop/queue/MaxPackets", UintegerValue(1000));
            Config::Set("/NodeList/" + std::to_string(staNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop/txopLimit", TimeValue(MicroSeconds(configs[i].txopDurationUs)));
        }

        // Mobility
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP
        positionAlloc->Add(Vector(configs[i].distance, 0.0, 0.0)); // STA
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes.Get(i));
        mobility.Install(staNodes.Get(i));

        // IP addressing
        std::ostringstream subnet;
        subnet << "10." << i << ".0.0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer intf = address.Assign(apDevice);
        intf = address.Assign(staDevice);
        interfaces.push_back(intf);

        // Applications
        uint16_t port = 9;
        UdpServerHelper server(port);
        serverApps.Add(server.Install(apNodes.Get(i)));

        UdpClientHelper client(intf.GetAddress(0), port);
        client.SetAttribute("MaxBytes", UintegerValue(0)); // Infinite
        client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        client.SetAttribute("PacketSize", UintegerValue(1400));
        clientApps.Add(client.Install(staNodes.Get(i)));

        // Trace TXOP durations
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(apNodes.Get(i)->GetId()) + "/DeviceList/0/$ns3::WifiNetDevice/Mac/Txop/txopUsed", MakeBoundCallback(&PrintTxop, DynamicCast<WifiNetDevice>(apDevice.Get(0))));
    }

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Run();

    for (size_t i = 0; i < 4; ++i) {
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(interfaces[i].Get(0)->GetObject<Ipv4L3Protocol>()->GetClassifier());
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.GetMonitor();
        monitor->CheckForLostPackets();

        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
            if (t.destinationPort == 9) {
                NS_LOG_UNCOND(configs[i].name << ": Throughput: " << (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds())) / 1e6 << " Mbps");
                Time maxTxop = Seconds(0);
                // Retrieve max TXOP from configuration
                if (configs[i].txopDurationUs > 0) {
                    maxTxop = MicroSeconds(configs[i].txopDurationUs);
                } else {
                    maxTxop = MicroSeconds(3264); // Default TXOP for AC_BE
                }
                NS_LOG_UNCOND(configs[i].name << ": Max TXOP Duration: " << maxTxop.As(Time::US) << " us");
            }
        }
    }

    Simulator::Destroy();
    return 0;
}