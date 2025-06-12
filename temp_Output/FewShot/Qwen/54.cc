#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiGoodputSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472; // bytes
    double simulationTime = 10; // seconds
    double distance = 1.0; // meters
    bool enableRtsCts = false;
    uint8_t rtsThreshold = 65535;
    uint16_t channelWidth = 80;
    uint8_t mcs = 9;
    std::string guardInterval = "short";
    std::string phyMode = "HeMcs" + std::to_string(mcs);
    std::string trafficType = "udp";

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance in meters between station and AP", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
    cmd.AddValue("rtsThreshold", "RTS threshold (if RTS/CTS is enabled)", rtsThreshold);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80, 160)", channelWidth);
    cmd.AddValue("mcs", "Modulation and Coding Scheme index (0-9)", mcs);
    cmd.AddValue("guardInterval", "Guard interval type: 'short' or 'long'", guardInterval);
    cmd.AddValue("trafficType", "Traffic type: 'udp' or 'tcp'", trafficType);
    cmd.Parse(argc, argv);

    phyMode = "HeMcs" + std::to_string(mcs);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    SpectrumChannelHelper channelHelper = SpectrumChannelHelper::Default();
    channelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<SpectrumChannel> channel = channelHelper.Create();

    phy.SetChannel(channel);
    phy.SetErrorRateModel("ns3::NistErrorRateModel");

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    if (enableRtsCts) {
        mac.SetAttribute("RTSThreshold", UintegerValue(rtsThreshold));
    } else {
        mac.SetAttribute("RTSThreshold", UintegerValue(65535)); // disable RTS/CTS
    }

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi")),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("ns3-wifi")));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    uint16_t port = 9;

    ApplicationContainer sinkApp;
    ApplicationContainer clientApp;

    if (trafficType == "udp") {
        UdpServerHelper server(port);
        sinkApp = server.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime));

        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApp = client.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    } else if (trafficType == "tcp") {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = sink.Install(wifiApNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simulationTime));

        OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("1000Mb/s")));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));

        clientApp = client.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        if (i->first > 1) { // ignore first flow (ARP)
            double throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000; // Mbps
            std::cout << "Distance=" << distance << "m, RTSCts=" << (enableRtsCts ? "ON" : "OFF")
                      << ", ChannelWidth=" << channelWidth << "MHz"
                      << ", MCS=" << static_cast<uint32_t>(mcs)
                      << ", GI=" << guardInterval
                      << ", Traffic=" << trafficType
                      << ", Goodput=" << throughput << " Mbps" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}