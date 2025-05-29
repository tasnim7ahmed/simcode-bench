#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/nstime.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi80211axSimulation");

int main(int argc, char *argv[]) {
    bool verbose = false;
    uint32_t nsta = 1;
    double distance = 10.0;
    bool dlMuApAcks = false;
    bool rtsCts = false;
    bool ht8080 = false;
    bool extendedBA = false;
    bool ulOfdma = false;
    std::string phyModel = "Yans"; // Yans or Spectrum
    std::string band = "5GHz";
    uint32_t payloadSize = 1472;
    std::string rate = "Tcp";
    std::string mcsValues = "0";
    std::string channelWidths = "20";
    double targetThroughputMbps = 10.0;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true.", verbose);
    cmd.AddValue("nsta", "Number of stations.", nsta);
    cmd.AddValue("distance", "Distance between AP and stations (meters).", distance);
    cmd.AddValue("dlMuApAcks", "Enable DL MU AP Acks.", dlMuApAcks);
    cmd.AddValue("rtsCts", "Enable RTS/CTS.", rtsCts);
    cmd.AddValue("ht8080", "Enable 80+80 MHz channel.", ht8080);
    cmd.AddValue("extendedBA", "Enable extended Block Ack.", extendedBA);
    cmd.AddValue("ulOfdma", "Enable UL OFDMA.", ulOfdma);
    cmd.AddValue("phyModel", "Phy model (Yans or Spectrum).", phyModel);
    cmd.AddValue("band", "Frequency band (2.4GHz, 5GHz, 6GHz).", band);
    cmd.AddValue("payloadSize", "Payload size in bytes.", payloadSize);
    cmd.AddValue("rate", "Transport protocol (Tcp or Udp).", rate);
    cmd.AddValue("mcsValues", "MCS values to test (comma separated).", mcsValues);
    cmd.AddValue("channelWidths", "Channel widths to test (comma separated).", channelWidths);
    cmd.AddValue("targetThroughputMbps", "Target throughput in Mbps.", targetThroughputMbps);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("Wifi80211axSimulation", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::WifiMacQueue::MaxPackets", UintegerValue(10000));

    NodeContainer staNodes;
    staNodes.Create(nsta);
    NodeContainer apNode;
    apNode.Create(1);

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ax);

    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    if (phyModel == "Spectrum") {
       phyHelper = SpectrumWifiPhyHelper::Default();
    }

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansPropagationLossModel propagationLossModel = YansPropagationLossModel::Default();
    channelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
    channelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    phyHelper.SetChannel(channelHelper.Create());

    WifiMacHelper macHelper;
    Ssid ssid = Ssid("ns-3-ssid");

    NetDeviceContainer staDevices;
    NetDeviceContainer apDevices;

    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false));
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "BeaconInterval", TimeValue(Seconds(0.05)));
    apDevices = wifiHelper.Install(phyHelper, macHelper, apNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                   "MinX", DoubleValue(0.0),
                                   "MinY", DoubleValue(0.0),
                                   "DeltaX", DoubleValue(distance),
                                   "DeltaY", DoubleValue(0.0),
                                   "GridWidth", UintegerValue(nsta),
                                   "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    mobility.SetPositionAllocator("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);

    InternetStackHelper internet;
    internet.Install(staNodes);
    internet.Install(apNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staNodeInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apNodeInterfaces = address.Assign(apDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    ApplicationContainer sinkApp;
    ApplicationContainer clientApp;

    if (rate == "Tcp") {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        sinkApp = sink.Install(apNode.Get(0));

        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(apNodeInterfaces.GetAddress(0), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        clientApp = source.Install(staNodes);
    } else {
        UdpEchoServerHelper echoServer(port);
        sinkApp = echoServer.Install(apNode.Get(0));

        UdpEchoClientHelper echoClient(apNodeInterfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApp = echoClient.Install(staNodes);
    }

    sinkApp.Start(Seconds(1.0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));
    sinkApp.Stop(Seconds(10.0));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow ID: " << i->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps\n";
    }

    Simulator::Destroy();
    return 0;
}