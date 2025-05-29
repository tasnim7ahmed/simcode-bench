#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/netanim-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQosExample");

int main(int argc, char *argv[]) {

    bool tracing = false;
    uint32_t payloadSize = 1472;
    double distance = 1.0;
    bool rtsCtsEnabled = false;
    double simulationTime = 10.0;
    std::string dataRate = "54Mbps";
    std::string phyMode = "OfdmRate54Mbps";
    bool pcapTracing = false;

    CommandLine cmd;
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.AddValue("rtsCtsEnabled", "Enable RTS/CTS", rtsCtsEnabled);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate for OnOff application", dataRate);
    cmd.AddValue("phyMode", "Wifi Phy mode", phyMode);
    cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);

    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::WifiMacQueue::MaxSize", StringValue ("64000"));
    Config::SetDefault ("ns3::WifiMacQueue::Mode", EnumValue (WifiMacQueue::ADAPTIVE));

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];
    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];

    apNodes[0].Create(1);
    staNodes[0].Create(1);
    apNodes[1].Create(1);
    staNodes[1].Create(1);
    apNodes[2].Create(1);
    staNodes[2].Create(1);
    apNodes[3].Create(1);
    staNodes[3].Create(1);

    YansWifiChannelHelper channel[4];
    YansWifiPhyHelper phy[4];

    for (int i = 0; i < 4; ++i) {
        channel[i] = YansWifiChannelHelper::Default();
        phy[i] = YansWifiPhyHelper::Default();
        channel[i].AddPropagationLoss("ns3::FriisPropagationLossModel");
        channel[i].AddPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        phy[i].SetChannel(channel[i].Create());
        phy[i].Set("TxPowerStart", DoubleValue(20));
        phy[i].Set("TxPowerEnd", DoubleValue(20));

        WifiHelper wifi;
        wifi.SetStandard(WIFI_PHY_STANDARD_80211n);

        WifiMacHelper mac;
        Ssid ssid = Ssid("ns3-ssid");

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi.Install(phy[i], mac, staNodes[i]);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconGeneration", BooleanValue(true),
                    "QosSupported", BooleanValue(true));

        apDevices[i] = wifi.Install(phy[i], mac, apNodes[i]);

        if(pcapTracing) {
           phy[i].EnablePcap("wifi-qos-" + std::to_string(i), apDevices[i].Get(0));
           phy[i].EnablePcap("wifi-qos-" + std::to_string(i), staDevices[i].Get(0));
        }
    }

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    for (int i = 0; i < 4; ++i) {
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);

        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(distance),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
        mobility.Install(staNodes[i]);
    }

    // Internet Stack
    InternetStackHelper stack;
    for (int i = 0; i < 4; ++i) {
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);
    }

    Ipv4AddressHelper address[4];
    Ipv4InterfaceContainer apInterface[4];
    Ipv4InterfaceContainer staInterface[4];

    address[0].SetBase("10.1.1.0", "255.255.255.0");
    apInterface[0] = address[0].Assign(apDevices[0]);
    staInterface[0] = address[0].Assign(staDevices[0]);

    address[1].SetBase("10.1.2.0", "255.255.255.0");
    apInterface[1] = address[1].Assign(apDevices[1]);
    staInterface[1] = address[1].Assign(staDevices[1]);

    address[2].SetBase("10.1.3.0", "255.255.255.0");
    apInterface[2] = address[2].Assign(apDevices[2]);
    staInterface[2] = address[2].Assign(staDevices[2]);

    address[3].SetBase("10.1.4.0", "255.255.255.0");
    apInterface[3] = address[3].Assign(apDevices[3]);
    staInterface[3] = address[3].Assign(staDevices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Applications

    uint16_t port = 9;

    UdpServerHelper server(port);

    ApplicationContainer serverApps[4];

    for (int i = 0; i < 4; ++i) {
        serverApps[i] = server.Install(apNodes[i].Get(0));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime + 1));
    }

    OnOffHelper onoff[4];
    onoff[0].SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff[0].SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff[0].SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff[0].SetAttribute("DataRate", StringValue(dataRate));
    onoff[0].SetAttribute("MaxBytes", UintegerValue(0));
    onoff[0].SetAttribute("Priority", UintegerValue(0));

    onoff[1].SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff[1].SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff[1].SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff[1].SetAttribute("DataRate", StringValue(dataRate));
    onoff[1].SetAttribute("MaxBytes", UintegerValue(0));
    onoff[1].SetAttribute("Priority", UintegerValue(2));

    onoff[2].SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff[2].SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff[2].SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff[2].SetAttribute("DataRate", StringValue(dataRate));
    onoff[2].SetAttribute("MaxBytes", UintegerValue(0));
    onoff[2].SetAttribute("Priority", UintegerValue(0));

    onoff[3].SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff[3].SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff[3].SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff[3].SetAttribute("DataRate", StringValue(dataRate));
    onoff[3].SetAttribute("MaxBytes", UintegerValue(0));
    onoff[3].SetAttribute("Priority", UintegerValue(2));

    ApplicationContainer clientApps[4];

    for (int i = 0; i < 4; ++i) {
        AddressValue remoteAddress(InetSocketAddress(apInterface[i].GetAddress(0), port));
        onoff[i].SetAttribute("Remote", remoteAddress);
        clientApps[i] = onoff[i].Install(staNodes[i].Get(0));
        clientApps[i].Start(Seconds(1.0));
        clientApps[i].Stop(Seconds(simulationTime));
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 2));

    if (tracing) {
        AsciiTraceHelper ascii;
        phy[0].EnableAsciiAll(ascii.Create("wifi-qos.tr"));
    }

    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}