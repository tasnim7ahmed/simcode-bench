#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/spectrum-wifi-phy-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-mac-helper.h"
#include <vector>
#include <string>
#include <iostream>

using namespace ns3;

int main (int argc, char *argv[])
{
    uint32_t nSta = 4;
    std::string phyModel = "Yans";
    std::string wifiBand = "5GHz";
    std::string dataRate = "HE_MCS9";
    std::string ackSequence = "Standard";
    bool enableRtsCts = false;
    bool enable8080 = false;
    bool enableExtBA = false;
    bool enableUlOfdma = false;
    bool useUdp = true;
    double distance = 10.0;
    uint32_t payloadSize = 1472;
    double targetThroughput = 100e6;
    std::string channelWidth = "80";
    std::string guardInterval = "800ns"; // can be "800ns", "1600ns", "3200ns"
    double simulationTime = 10.0;
    uint8_t mcsMin = 0;
    uint8_t mcsMax = 9;
    std::string outputDir = "results/";

    CommandLine cmd;
    cmd.AddValue ("nSta", "Number of stations", nSta);
    cmd.AddValue ("phyModel", "PHY model: Yans or Spectrum", phyModel);
    cmd.AddValue ("wifiBand", "Wi-Fi band: 2.4GHz, 5GHz, 6GHz", wifiBand);
    cmd.AddValue ("dataRate", "DataRate (HE_MCS0-HE_MCS11)", dataRate);
    cmd.AddValue ("ackSequence", "Ack sequence: Standard/NoAck", ackSequence);
    cmd.AddValue ("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue ("enable8080", "Enable 80+80 MHz", enable8080);
    cmd.AddValue ("enableExtBA", "Enable extended block ack", enableExtBA);
    cmd.AddValue ("enableUlOfdma", "Enable UL OFDMA", enableUlOfdma);
    cmd.AddValue ("useUdp", "UDP(true) or TCP(false) flows", useUdp);
    cmd.AddValue ("distance", "Distance between AP and each STA (meters)", distance);
    cmd.AddValue ("payloadSize", "UDP/TCP packet payload size (bytes)", payloadSize);
    cmd.AddValue ("targetThroughput", "Offered load per flow (bps)", targetThroughput);
    cmd.AddValue ("channelWidth", "20,40,80,160,8080", channelWidth);
    cmd.AddValue ("guardInterval", "Guard Interval: 800ns/1600ns/3200ns", guardInterval);
    cmd.AddValue ("simulationTime", "Simulation time (s)", simulationTime);
    cmd.AddValue ("mcsMin", "Start MCS value", mcsMin);
    cmd.AddValue ("mcsMax", "End MCS value (inclusive)", mcsMax);
    cmd.AddValue ("outputDir", "Output directory", outputDir);
    cmd.Parse (argc, argv);

    std::vector<uint8_t> mcsList;
    for (uint8_t m = mcsMin; m <= mcsMax; ++m)
        mcsList.push_back(m);

    uint16_t channelNumber = 36;
    double centerFreq = 5.18e9;
    if (wifiBand == "2.4GHz")
    {
        channelNumber = 1;
        centerFreq = 2.412e9;
    }
    else if (wifiBand == "5GHz")
    {
        channelNumber = 36;
        centerFreq = 5.18e9;
    }
    else if (wifiBand == "6GHz")
    {
        channelNumber = 1;
        centerFreq = 6.005e9;
    }

    uint16_t chWidthVal = 80;
    if (channelWidth == "20")      chWidthVal = 20;
    else if (channelWidth == "40") chWidthVal = 40;
    else if (channelWidth == "80") chWidthVal = 80;
    else if (channelWidth == "160") chWidthVal = 160;
    else if (channelWidth == "8080") chWidthVal = 160;

    std::string gi = "HE_GI_0_8us";
    if (guardInterval == "1600ns") gi = "HE_GI_1_6us";
    else if (guardInterval == "3200ns") gi = "HE_GI_3_2us";

    for (uint8_t mcsIdx : mcsList)
    {
        NodeContainer wifiStaNodes, wifiApNode;
        wifiStaNodes.Create (nSta);
        wifiApNode.Create (1);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector (0.0, 0.0, 1.5));
        for (uint32_t i = 0; i < nSta; ++i)
            positionAlloc->Add (Vector (distance * (1+i), 0.0, 1.5));
        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (wifiApNode);
        mobility.Install (wifiStaNodes);

        Ssid ssid = Ssid ("he-wifi");

        WifiHelper wifi;
        wifi.SetStandard (WIFI_STANDARD_80211ax);

        if (enableRtsCts)
            Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
        else
            Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));

        std::string mcsStr = std::to_string(mcsIdx);
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue ("HeMcs" + mcsStr),
                                     "ControlMode", StringValue ("HeMcs" + mcsStr));
        
        WifiMacHelper mac;
        YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper phy;
        SpectrumWifiPhyHelper spectrumPhy;
        NetDeviceContainer staDevices, apDevice;
        if (phyModel == "Spectrum")
        {
            spectrumPhy = SpectrumWifiPhyHelper::Default ();
            Ptr<SpectrumChannel> sc = CreateObject<SingleModelSpectrumChannel> ();
            spectrumPhy.SetChannel (sc);
            spectrumPhy.Set ("Frequency", UintegerValue ((uint32_t)(centerFreq/1e6)));
            spectrumPhy.SetPcapDataLinkType (SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);
            spectrumPhy.Set ("ChannelWidth", UintegerValue (chWidthVal));
            spectrumPhy.Set ("GuardInterval", StringValue (gi));
            if (enable8080 && chWidthVal == 160)
                spectrumPhy.Set ("ChannelWidth", UintegerValue (160));
            mac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "EnableSuProtection", BooleanValue (true));
            staDevices = wifi.Install (spectrumPhy, mac, wifiStaNodes);
            mac.SetType ("ns3::ApWifiMac",
                         "Ssid", SsidValue (ssid));
            apDevice = wifi.Install (spectrumPhy, mac, wifiApNode);
        }
        else
        {
            phy = YansWifiPhyHelper::Default ();
            phy.SetChannel (yansChannel.Create ());
            phy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
            phy.Set ("ChannelWidth", UintegerValue (chWidthVal));
            phy.Set ("GuardInterval", StringValue (gi));
            if (enable8080 && chWidthVal == 160)
                phy.Set ("ChannelWidth", UintegerValue (160));
            mac.SetType ("ns3::StaWifiMac",
                         "Ssid", SsidValue (ssid),
                         "EnableSuProtection", BooleanValue (true));
            staDevices = wifi.Install (phy, mac, wifiStaNodes);
            mac.SetType ("ns3::ApWifiMac",
                         "Ssid", SsidValue (ssid));
            apDevice = wifi.Install (phy, mac, wifiApNode);
        }

        if (enableExtBA)
        {
            Config::SetDefault ("ns3::WifiRemoteStationManager::BlockAckInactivityTimeout", UintegerValue (1000));
        }

        if (enableUlOfdma)
        {
            for (auto nd = staDevices.Begin (); nd != staDevices.End (); ++nd)
                (*nd)->GetObject<WifiNetDevice> ()->SetAttribute ("EnableUlOfdma", BooleanValue (true));
            apDevice.Get (0)->GetObject<WifiNetDevice> ()->SetAttribute ("EnableUlOfdma", BooleanValue (true));
        }

        InternetStackHelper stack;
        stack.Install (wifiApNode);
        stack.Install (wifiStaNodes);

        Ipv4AddressHelper address;
        address.SetBase ("192.168.1.0", "255.255.255.0");
        Ipv4InterfaceContainer apIf = address.Assign (apDevice);
        Ipv4InterfaceContainer staIf = address.Assign (staDevices);

        uint16_t port = 9000;
        ApplicationContainer serverApp, clientApp;

        if (useUdp)
        {
            UdpServerHelper server (port);
            serverApp = server.Install (wifiStaNodes);
            serverApp.Start (Seconds (0.1));
            serverApp.Stop (Seconds (simulationTime+1));

            for (uint32_t i = 0; i < nSta; ++i)
            {
                UdpClientHelper client (staIf.GetAddress (i), port);
                client.SetAttribute ("MaxPackets", UintegerValue (0));
                client.SetAttribute ("Interval", TimeValue (Seconds ((double)payloadSize*8 / targetThroughput)));
                client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                ApplicationContainer ac = client.Install (wifiApNode.Get (0));
                ac.Start (Seconds (0.2));
                ac.Stop (Seconds (simulationTime+1));
                clientApp.Add (ac);
            }
        }
        else
        {
            for (uint32_t i = 0; i < nSta; ++i)
            {
                Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port + i));
                PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
                ApplicationContainer sinkApp = sinkHelper.Install (wifiStaNodes.Get (i));
                sinkApp.Start (Seconds (0.));
                sinkApp.Stop (Seconds (simulationTime+1));
                serverApp.Add (sinkApp);

                OnOffHelper onoff ("ns3::TcpSocketFactory", Address (InetSocketAddress (staIf.GetAddress (i), port + i)));
                onoff.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                onoff.SetAttribute ("DataRate", DataRateValue (DataRate ((uint64_t)targetThroughput)));
                ApplicationContainer srcApp = onoff.Install (wifiApNode.Get (0));
                srcApp.Start (Seconds (0.2));
                srcApp.Stop (Seconds (simulationTime+1));
                clientApp.Add (srcApp);
            }
        }

        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

        Simulator::Stop (Seconds (simulationTime + 1));
        Simulator::Run ();

        monitor->CheckForLostPackets ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
        std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
        double sumThroughput = 0.0;
        for (auto &&flow : stats)
        {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (flow.first);
            if (t.destinationPort == port || (t.destinationPort >= port && t.destinationPort < port+nSta))
            {
                double throughput = (double)flow.second.rxBytes * 8.0 / (simulationTime * 1e6);
                sumThroughput += throughput;
            }
        }

        std::cout << "MCS=" << unsigned(mcsIdx)
                  << ", ChannelWidth=" << chWidthVal
                  << ", GuardInterval=" << guardInterval
                  << ", AggregateThroughput=" << sumThroughput << " Mbps" << std::endl;

        Simulator::Destroy ();
    }

    return 0;
}