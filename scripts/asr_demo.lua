-- asr text for one round
local asr_text = nil;


local ivr_sound = "ivr/ivr-did_you_mean_to_press_key.wav";
local ivr_sound_2 = "ivr-please_enter_extension_followed_by_pound.wav";


-- This is the input callback used by dtmf or any other events on this session such as ASR.
function onInput(s, type, obj)
    -- freeswitch.consoleLog("info", "Callback with type " .. type .. "\n");
    if (type == "dtmf") then
        freeswitch.consoleLog("info", "DTMF Digit: " .. obj.digit .. "\n");
    elseif (type == "event") then
        local event = obj:getHeader("Speech-Type");
        if (event == "begin-speaking") then
            freeswitch.consoleLog("info", "speaking=" .. obj:serialize() .. "\n");
            -- Return break on begin-speaking events to stop playback of the fire or tts.
            return "break";
        end

        if ( event == "detected-speech" ) then
            -- freeswitch.consoleLog("info", "\n" .. obj:serialize() .. "\n");
            local text = obj:getBody();
            if ( text ~= "(null)" ) then
                -- Pause speech detection (this is on auto but pausing it just in case)
                session:execute("detect_speech", "pause");

                -- Parse the results from the event into the results table for later use.
                -- results = getResults(obj:getBody());

                -- set the global asr text for later use
                asr_text = text;
            end
            -- return "break";
        end
    end
end


session:answer();

-- local vars = {
--     "destination_number", "caller_id_name", "caller_id_number",
--     "network_addr", "uuid"
-- };
-- 
-- for k, v in pairs(vars) do
--     print(v .. ": " .. session:getVariable(v));
-- end


-- Define the TTS engine
--session:set_tts_params("yytts", "zhilingf");
-- Register the input callback
session:setInputCallback("onInput");
-- Sleep a little bit to get media time to be fully up
session:sleep(100);
--session:speak("我是原语智能的电话系统，请说");
session:execute("playback", ivr_sound);
session:execute("detect_speech", "whisper pizza delivery");
local caller_id_number = session:getVariable("caller_id_number");

-- keep the thread alive
while ( session:ready() == true ) do
    if ( asr_text == nil ) then
        session:sleep(20);
        freeswitch.consoleLog("debug", " Lets  sleep over the channel because there is no asr_text\n");
    elseif ( asr_text == "" ) then
        -- session:speak("没听清你说什么哦，请再说一遍");
	session:execute("playback", ivr_sound_2);
        asr_text = nil;
        freeswitch.consoleLog("debug", " Lets resume detection again because there is empty asr_text \n");
        session:execute("detect_speech", "resume");
    else
        -- do your NLU here ?

        -- echo back the recognition result
        freeswitch.consoleLog("info", asr_text);
        asr_text = nil;

        freeswitch.consoleLog("debug", " Lets resume detection again and set asr_text to nill \n");
        session:execute("detect_speech", "resume");
    end
end

-- stop the detect_speech and hangup
session:execute("detect_speech", "stop");
session:sleep(1000);
session:hangup();
